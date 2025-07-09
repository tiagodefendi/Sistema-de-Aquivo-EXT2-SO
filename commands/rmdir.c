#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "commands.h"
#include "utils.h"

/**
 * @brief Libera uma cadeia de blocos indiretos.
 *
 * Esta função libera todos os blocos em uma cadeia de blocos indiretos, recursivamente,
 * até a profundidade especificada. Se a profundidade for 1, libera blocos indiretos simples;
 * se for 2, libera blocos indiretos duplos; e se for 3, libera blocos indiretos triplos.
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param blk   Número do bloco a ser liberado.
 * @param depth Profundidade da cadeia de blocos indiretos.
 *
 * @return Retorna 0 em caso de sucesso ou 1 em caso de erro.
 */
static int free_indirect_chain(ext2_fs_t *fs, uint32_t blk, int depth)
{
    if (!blk) // Se o bloco for nulo, não há nada a liberar
    {
        return EXIT_SUCCESS;
    }
    uint32_t ptrs[EXT2_BLOCK_SIZE / sizeof(uint32_t)]; // Array para armazenar os ponteiros dos blocos indiretos
    if (fs_read_block(fs, blk, ptrs) < 0)              // Lê o bloco indireto
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    for (int i = 0; i < (int)(EXT2_BLOCK_SIZE / sizeof(uint32_t)); i++) // Percorre todos os ponteiros no bloco indireto
    {
        uint32_t b = ptrs[i];
        if (!b)
            continue;
        if (depth > 1) // Se a profundidade for maior que 1, libera blocos indiretos recursivamente
        {
            if (free_indirect_chain(fs, b, depth - 1) == EXIT_FAILURE) // Libera blocos indiretos
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
        }
        else // Se a profundidade for 1, libera blocos indiretos simples
        {
            if (fs_free_block(fs, b) < 0)
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
        }
    }
    if (fs_free_block(fs, blk) < 0) // Libera o bloco indireto em si
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @brief Libera os blocos de dados de um inode e atualiza o bitmap.
 *
 * Esta função libera todos os blocos diretos, indiretos simples, duplos e triplos de um inode,
 * além de atualizar o bitmap de blocos livres do sistema de arquivos.
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param inode Ponteiro para o inode cujos blocos serão liberados.
 *
 * @return Retorna 0 em caso de sucesso ou 1 em caso de erro.
 */
static int free_inode_blocks_logged(ext2_fs_t *fs, struct ext2_inode *inode)
{
    for (int i = 0; i < 12; i++) // Libera blocos diretos
    {
        uint32_t b = inode->i_block[i];
        if (!b)
        {
            continue;
        }
        if (fs_free_block(fs, b) < 0)
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }
    }

    if (free_indirect_chain(fs, inode->i_block[12], 1) == EXIT_FAILURE) // Libera indireto simples
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (free_indirect_chain(fs, inode->i_block[13], 2) == EXIT_FAILURE) // Libera indireto duplo
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (free_indirect_chain(fs, inode->i_block[14], 3) == EXIT_FAILURE) // Libera indireto triplo
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Verifica se um diretório está vazio.
 *
 * Esta função verifica se um diretório está vazio, ou seja, se não contém
 * entradas de diretório além de "." e "..".
 *
 * @param fs        Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode Ponteiro para o inode do diretório a ser verificado.
 *
 * @return Retorna 1 se o diretório estiver vazio, 0 se não estiver e -1 em caso de erro.
 */
static int is_directory_empty(ext2_fs_t *fs, struct ext2_inode *dir_inode)
{
    uint8_t buf[EXT2_BLOCK_SIZE]; // Buffer para armazenar os blocos lidos
    for (int i = 0; i < 12; i++)  // Percorre todos os blocos diretos
    {
        uint32_t b = dir_inode->i_block[i]; // Obtém o número do bloco
        if (!b)
        {
            continue;
        }
        if (fs_read_block(fs, b, buf) < 0) // Lê o bloco
        {
            return -1;
        }

        uint32_t off = 0;
        while (off < EXT2_BLOCK_SIZE) // Percorre todas as entradas do diretório
        {
            struct ext2_dir_entry *e = (void *)(buf + off); // Obtém a entrada de diretório
            if (e->rec_len == 0)                            // Se o comprimento da entrada for 0, termina a iteração
            {
                break;
            }
            // Verifica se a entrada é um diretório válido
            if (e->inode && !(e->name_len == 1 && e->name[0] == '.') && !(e->name_len == 2 && e->name[0] == '.' && e->name[1] == '.'))
            {
                return 0;
            }
            off += e->rec_len; // Avança para a próxima entrada
        }
    }
    return 1; // Diretório está vazio
}

/**
 * @brief Remove uma entrada de diretório pelo número do inode.
 *
 * Esta função percorre os blocos diretos de um inode de diretório e remove a entrada correspondente ao inode alvo.
 * Se a entrada for encontrada, ela é zerada e o bloco é atualizado.
 *
 * @param fs            Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param parent_inode  Ponteiro para o inode do diretório onde a entrada será removida.
 * @param target_ino    Número do inode da entrada a ser removida.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se a entrada não for encontrada).
 */
static int dir_remove_entry_rm(ext2_fs_t *fs, struct ext2_inode *parent_inode, uint32_t target_ino)
{
    uint8_t buf[EXT2_BLOCK_SIZE]; // Buffer para armazenar o bloco lido
    for (int i = 0; i < 12; i++)  // Percorre os blocos diretos do diretório
    {
        uint32_t b = parent_inode->i_block[i]; // Obtém o número do bloco
        if (!b)
            continue;
        if (fs_read_block(fs, b, buf) < 0) // Lê o bloco do diretório
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }

        uint32_t off = 0;
        struct ext2_dir_entry *prev = NULL; // Variável para armazenar a entrada anterior
        while (off < EXT2_BLOCK_SIZE)       // Percorre o bloco até o final
        {
            struct ext2_dir_entry *e = (void *)(buf + off); // Obtém a entrada de diretório atual

            if (e->rec_len == 0) // Se a entrada não tiver comprimento, significa que não há mais entradas
                break;
            if (e->inode == target_ino) // Verifica se a entrada corresponde ao inode alvo
            {
                if (prev) // Se houver uma entrada anterior, funde o espaço da entrada removida no rec_len da anterior
                    prev->rec_len += e->rec_len;
                else // Se for a primeira entrada do bloco, marcaremos ela como livre estendendo-a até o fim do bloco
                {
                    e->inode = 0;
                    e->rec_len = EXT2_BLOCK_SIZE;
                }
                if (fs_write_block(fs, b, buf) < 0) // Atualiza o bloco com a entrada removida
                {
                    print_error(ERROR_UNKNOWN);
                    return EXIT_FAILURE;
                }
                return EXIT_SUCCESS;
            }
            prev = e;          // Atualiza a entrada anterior
            off += e->rec_len; // Avança para a próxima entrada
        }
    }
    return EXIT_FAILURE; // Se a entrada não for encontrada, retorna falha
}

/**
 * @brief Remove um diretório vazio.
 *
 * Esta função remove um diretório vazio do sistema de arquivos EXT2.
 * Ela verifica se o diretório está vazio, remove a entrada do diretório pai
 * e libera os blocos e inodes associados ao diretório.
 *
 * @param argc Número de argumentos da linha de comando.
 * @param argv Argumentos da linha de comando.
 * @param fs   Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd  Ponteiro para o inode do diretório atual.
 *
 * @return Retorna 0 em caso de sucesso ou 1 em caso de erro.
 */
int cmd_rmdir(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2) // Verifica se o número de argumentos está correto
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }
    char *full_path = fs_join_path(fs, *cwd, argv[1]); // Junta o caminho atual com o nome do diretório
    if (!full_path)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    uint32_t dir_ino;
    if (fs_path_resolve(fs, full_path, &dir_ino) < 0) // Resolve o caminho para obter o inode do diretório
    {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }
    struct ext2_inode dir_inode;
    if (fs_read_inode(fs, dir_ino, &dir_inode) < 0 || !ext2_is_dir(&dir_inode)) // Lê o inode do diretório e verifica se é um diretório
    {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }
    int empty = is_directory_empty(fs, &dir_inode); // Verifica se o diretório está vazio
    if (empty < 0)
    {
        free(full_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (!empty) // Se o diretório não estiver vazio, exibe erro
    {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_EMPTY);
        return EXIT_FAILURE;
    }

    char *parent_path = strdup(full_path);
    char *slash = strrchr(parent_path, '/');
    if (slash && slash != parent_path)
        *slash = '\0';
    else
        strcpy(parent_path, "/");

    uint32_t parent_ino;
    if (fs_path_resolve(fs, parent_path, &parent_ino) < 0) // Resolve o inode do diretório pai
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    struct ext2_inode parent_inode;
    fs_read_inode(fs, parent_ino, &parent_inode);                        // Lê o inode do diretório pai
    if (dir_remove_entry_rm(fs, &parent_inode, dir_ino) == EXIT_FAILURE) // Remove a entrada do diretório no inode do pai
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    parent_inode.i_links_count--;
    fs_write_inode(fs, parent_ino, &parent_inode);                // Atualiza o inode do diretório pai
    if (free_inode_blocks_logged(fs, &dir_inode) == EXIT_FAILURE) // Libera os blocos do inode do diretório
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (fs_free_inode(fs, dir_ino) < 0) // Libera o inode do diretório
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Sincroniza o superbloco
    fs_sync_super(fs);
    free(full_path);
    free(parent_path);
    return EXIT_SUCCESS;
}
