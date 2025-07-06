#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "commands.h"

/**
 * @brief Verifica se um diretório EXT2 está vazio (exceto "." e "..")
 *
 * @param fs        Ponteiro para o sistema de arquivos EXT2
 * @param dir_inode Ponteiro para o inode do diretório
 * @return 1 se estiver vazio, 0 se não estiver, -1 em caso de erro
 */
static int is_directory_empty(ext2_fs_t *fs, struct ext2_inode *dir_inode)
{
    uint8_t buf[EXT2_BLOCK_SIZE];

    // Percorre os blocos diretos (0 a 11) do inode do diretório
    for (int i = 0; i < 12; ++i)
    {
        uint32_t blk = dir_inode->i_block[i];
        if (!blk) continue; // Pula blocos não alocados

        // Lê o bloco do diretório
        if (fs_read_block(fs, blk, buf) < 0)
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }

        // Percorre as entradas de diretório dentro do bloco
        uint32_t off = 0;
        while (off < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *e = (void *)(buf + off);
            if (e->rec_len == 0) break; // Fim da lista de entradas

            // Verifica se é uma entrada válida e diferente de "." ou ".."
            if (e->inode != 0 && !(e->name_len == 1 && e->name[0] == '.') &&
                !(e->name_len == 2 && e->name[0] == '.' && e->name[1] == '.'))
            {
                return EXIT_SUCCESS; // Diretório não está vazio
            }

            off += e->rec_len; // Avança para a próxima entrada
        }
    }

    // Se não encontrou nada além de "." e "..", o diretório está vazio
    return EXIT_FAILURE;
}

/**
 * @brief   Remove uma entrada de diretório pelo número do inode
 *
 * @param fs           Ponteiro para o sistema de arquivos EXT2
 * @param parent_inode Ponteiro para o inode do diretório pai
 * @param target_ino   Número do inode a ser removido
 * @return 0 em sucesso, -1 em erro
 */
static int dir_remove_entry_rm(ext2_fs_t *fs, struct ext2_inode *parent_inode, uint32_t target_ino)
{
    uint8_t buf[EXT2_BLOCK_SIZE];

    // Percorre os blocos diretos do diretório pai
    for (int i = 0; i < 12; ++i)
    {
        uint32_t blk = parent_inode->i_block[i];
        if (!blk) continue;

        if (fs_read_block(fs, blk, buf) < 0)
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }

        uint32_t off = 0;
        struct ext2_dir_entry *prev = NULL; // Mantém ponteiro para a entrada anterior
        while (off < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *e = (void *)(buf + off);
            if (e->rec_len == 0) break;

            if (e->inode == target_ino)
            {
                if (prev)
                    // Junta a entrada removida ao espaço da anterior
                    prev->rec_len += e->rec_len;
                else
                {
                    // Se for a primeira entrada, apenas zera
                    e->inode = 0;
                    e->rec_len = EXT2_BLOCK_SIZE;
                }

                // Escreve as mudanças no disco
                if (fs_write_block(fs, blk, buf) < 0)
                {
                    print_error(ERROR_UNKNOWN);
                    return EXIT_FAILURE;
                }
                return EXIT_SUCCESS;
            }

            prev = e;
            off += e->rec_len;
        }
    }

    // Entrada com o inode alvo não foi encontrada
    print_error(ERROR_UNKNOWN);
    return EXIT_FAILURE;
}

/**
 * @brief   Comando 'rmdir' para remover um diretório vazio
 *
 * @param argc  Número de argumentos
 * @param argv  Array de argumentos
 * @param fs    Ponteiro pro sistema de arquivos EXT2
 * @param cwd   Ponteiro pro inode do diretório corrente
 * @return 0 em sucesso, -1 em erro
 */
int cmd_rmdir(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    // Verifica número de argumentos
    if (argc != 2)
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    // Concatena caminho completo com base no diretório atual
    char *full_path = fs_join_path(fs, *cwd, argv[1]);
    if (!full_path)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Resolve o caminho para obter o número do inode
    uint32_t dir_ino;
    if (fs_path_resolve(fs, full_path, &dir_ino) < 0)
    {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }

    // Lê o inode do diretório
    struct ext2_inode dir_inode;
    if (fs_read_inode(fs, dir_ino, &dir_inode) < 0)
    {
        free(full_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Verifica se realmente é um diretório
    if (!ext2_is_dir(&dir_inode))
    {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }

    // Verifica se o diretório está vazio
    int empty = is_directory_empty(fs, &dir_inode);
    if (empty < 0)
    {
        free(full_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (!empty)
    {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_EMPTY);
        return EXIT_FAILURE;
    }

    // Extrai o caminho do diretório pai
    char *parent_path = strdup(full_path);
    char *slash = strrchr(parent_path, '/');
    if (slash && slash != parent_path)
        *slash = '\0'; // Remove o nome do diretório final
    else
        strcpy(parent_path, "/"); // Caso seja a raiz

    // Resolve o caminho do diretório pai
    uint32_t parent_ino;
    if (fs_path_resolve(fs, parent_path, &parent_ino) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Lê o inode do diretório pai
    struct ext2_inode parent_inode;
    if (fs_read_inode(fs, parent_ino, &parent_inode) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Remove a entrada do diretório no pai
    if (dir_remove_entry_rm(fs, &parent_inode, dir_ino))
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Libera blocos de dados associados ao diretório
    if (free_inode_blocks(fs, &dir_inode) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Libera o próprio inode
    if (fs_free_inode(fs, dir_ino) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Libera memória alocada para caminhos
    free(full_path);
    free(parent_path);
    return EXIT_SUCCESS; // Diretório removido com sucesso
}
