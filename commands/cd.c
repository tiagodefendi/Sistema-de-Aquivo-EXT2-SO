#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

/**
 * @brief   Procura uma entrada de diretório pelo número do inode.
 *
 * Percorre os blocos de dados do diretório, procurando uma entrada cujo inode
 * corresponda ao especificado. Se encontrar, copia a entrada para 'out'.
 *
 * @param fs         Ponteiro para o sistema de arquivos EXT2.
 * @param dir_inode  Ponteiro para o inode do diretório a ser pesquisado.
 * @param tgt_ino    Número do inode a ser encontrado.
 * @param out        Ponteiro para onde a entrada encontrada será copiada.
 *
 * @return Retorna 0 se encontrado, -1 se não encontrado ou erro.
 */
static int find_entry_by_ino(ext2_fs_t *fs, struct ext2_inode *dir_inode, uint32_t tgt_ino, struct ext2_dir_entry *out)
{
    uint8_t buf[EXT2_BLOCK_SIZE]; // Buffer para leitura dos blocos do diretório
    for (int i = 0; i < 12; ++i)  // Percorre os blocos diretos do diretório
    {
        uint32_t bloco = dir_inode->i_block[i]; // Obtém o número do bloco
        if (!bloco)                             // Se o bloco não estiver alocado, pula para o próximo
            continue;

        if (fs_read_block(fs, bloco, buf) < 0) // Lê o bloco do diretório
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }

        uint32_t offset = 0;             // Inicializa o deslocamento para percorrer o bloco
        while (offset < EXT2_BLOCK_SIZE) // Percorre o bloco até o final
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + offset); // Obtém a entrada de diretório a partir do buffer

            if (entry->rec_len == 0) // Se o comprimento do registro for zero, significa que a entrada está corrompida
                break;

            if (entry->inode == tgt_ino) // Verifica se o inode da entrada corresponde ao inode alvo
            {
                memcpy(out, entry, sizeof(struct ext2_dir_entry));               // Copia a entrada encontrada
                size_t name_len = entry->name_len < 255 ? entry->name_len : 255; // Copia o nome separadamente, respeitando o tamanho
                memcpy(out->name, entry->name, name_len);                        // Copia o nome da entrada
                out->name[name_len] = '\0';                                      // Garante terminação
                return EXIT_SUCCESS;
            }
            offset += entry->rec_len;
        }
    }

    print_error(ERROR_DIRECTORY_NOT_FOUND);
    return EXIT_FAILURE;
}

/**
 * @brief   Muda o diretório corrente para o especificado.
 *
 * Esta função altera o diretório corrente do sistema de arquivos para o diretório
 * especificado pelo caminho relativo fornecido. Se o diretório não existir ou não for
 * um diretório, retorna erro.
 *
 * @param argc  Número de argumentos passados.
 * @param argv  Array de argumentos, onde argv[1] é o caminho do diretório.
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd   Ponteiro para o inode do diretório corrente.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_cd(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2) // Verifica se o número de argumentos é válido
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    char *abs_path = fs_join_path(fs, *cwd, argv[1]); // Resolve o caminho absoluto do diretório alvo
    if (!abs_path)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t dir_ino;                                // Variável para armazenar o inode do diretório
    if (fs_path_resolve(fs, abs_path, &dir_ino) < 0) // Resolve o inode do diretório
    {
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        free(abs_path);
        return EXIT_FAILURE;
    }

    struct ext2_inode dir_inode;                    // Estrutura para armazenar o inode do diretório
    if (fs_read_inode(fs, dir_ino, &dir_inode) < 0) // Lê o inode do diretório
    {
        print_error(ERROR_UNKNOWN);
        free(abs_path);
        return EXIT_FAILURE;
    }

    if (!ext2_is_dir(&dir_inode)) // Verifica se o inode é um diretório
    {
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        free(abs_path);
        return EXIT_FAILURE;
    }

    char *parent_path = strdup(abs_path);         // Buscar o nome no diretório pai
    char *last_slash = strrchr(parent_path, '/'); // Encontra a última barra no caminho
    if (last_slash && last_slash != parent_path)  // Se houver uma barra e não for o início do caminho
    {
        *last_slash = '\0';
    }
    else // Se não houver barra, é o diretório raiz
    {
        strcpy(parent_path, "/");
    }

    uint32_t parent_ino;                                    // Variável para armazenar o inode do diretório pai
    if (fs_path_resolve(fs, parent_path, &parent_ino) == 0) // Resolve o inode do diretório pai
    {
        struct ext2_inode parent_inode;                        // Estrutura para armazenar o inode do diretório pai
        if (fs_read_inode(fs, parent_ino, &parent_inode) == 0) // Lê o inode do diretório pai
        {
            struct ext2_dir_entry entry;                                    //     Estrutura para armazenar a entrada de diretório
            if (find_entry_by_ino(fs, &parent_inode, dir_ino, &entry) == 0) // Busca a entrada do diretório corrente no diretório pai
            {
                if (strcmp(entry.name, "..") == 0) // Se o nome da entrada é "..", buscar o nome real do pai no avô
                {
                    char *grandparent_path = strdup(parent_path);       // Copia o caminho do diretório pai
                    char *last_slash2 = strrchr(grandparent_path, '/'); // Encontra a última barra no caminho do diretório pai
                    if (last_slash2 && last_slash2 != grandparent_path) // Se houver uma barra e não for o início do caminho
                        *last_slash2 = '\0';                            // Remove a última barra
                    else                                                // Se não houver barra, é o diretório raiz
                        strcpy(grandparent_path, "/");                  // Define o caminho como raiz

                    uint32_t grandparent_ino;                                         // Variável para armazenar o inode do diretório avô
                    if (fs_path_resolve(fs, grandparent_path, &grandparent_ino) == 0) // Resolve o inode do diretório avô
                    {
                        struct ext2_inode grandparent_inode;                             // Estrutura para armazenar o inode do diretório avô
                        if (fs_read_inode(fs, grandparent_ino, &grandparent_inode) == 0) // Lê o inode do diretório avô
                        {
                            struct ext2_dir_entry real_entry;                                            // Estrutura para armazenar a entrada real do diretório pai
                            if (find_entry_by_ino(fs, &grandparent_inode, parent_ino, &real_entry) == 0) // Busca a entrada do diretório pai no diretório avô
                            {
                                print_entry(&real_entry); // Imprime o nome real do pai
                            }
                        }
                    }
                    free(grandparent_path); // Libera a memória alocada para o caminho do diretório avô
                }
                else // Se o nome da entrada não é "..", imprime o nome normal
                {
                    print_entry(&entry); // Nome normal
                }
            }
        }
    }

    free(parent_path); // Libera a memória alocada para o caminho do diretório pai
    *cwd = dir_ino;    // Atualiza o diretório corrente
    free(abs_path);    // Libera a memória alocada para o caminho absoluto
    return EXIT_SUCCESS;
}
