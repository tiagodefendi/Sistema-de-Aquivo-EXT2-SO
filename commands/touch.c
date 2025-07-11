#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "commands.h"

/**
 * @brief Cria um novo arquivo vazio ou atualiza o timestamp de um arquivo existente.
 *
 * Esta função implementa o comando `touch`, que cria um novo arquivo vazio
 * ou atualiza os timestamps de um arquivo existente no sistema de arquivos EXT2.
 *
 * @param argc Número de argumentos passados para o comando.
 * @param argv Array de strings contendo os argumentos do comando.
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o inode do diretório atual.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_touch(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    // Verifica se a quantidade de argumentos está correta
    if (argc != 2)
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    // Monta o caminho absoluto do arquivo a ser criado
    char *full_path = fs_join_path(fs, *cwd, argv[1]);
    if (!full_path)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Verifica se o arquivo já existe
    uint32_t existing_inode;
    if (fs_path_resolve(fs, full_path, &existing_inode) == 0)
    {
        free(full_path);
        print_error(ERROR_FILE_OR_DIRECTORY_ALREADY_EXISTS);
        return EXIT_FAILURE;
    }

    // Separa o caminho do diretório pai e o nome do arquivo
    char *last_slash = strrchr(full_path, '/');
    char *file_name = last_slash ? last_slash + 1 : full_path;
    char *parent_path = NULL;

    if (last_slash)
    {
        // Se o arquivo está na raiz
        if (last_slash == full_path)
            parent_path = strdup("/");
        else
        {
            // Se não está na raiz, separa o caminho do diretório pai
            *last_slash = '\0';
            parent_path = strdup(full_path);
            *last_slash = '/';
        }
    }
    else
    {
        // Se não há barra, pega o caminho do diretório atual
        parent_path = fs_get_path(fs, *cwd);
    }

    if (!parent_path)
    {
        free(full_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Resolve o inode do diretório pai
    uint32_t parent_inode_num;
    if (fs_path_resolve(fs, parent_path, &parent_inode_num) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Lê o inode do diretório pai
    struct ext2_inode parent_inode;
    if (fs_read_inode(fs, parent_inode_num, &parent_inode) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    // Verifica se o inode é realmente um diretório
    if (!ext2_is_dir(&parent_inode))
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }

    // Aloca um novo inode para o arquivo
    uint32_t new_inode_num;
    if (fs_alloc_inode(fs, EXT2_S_IFREG | 0644, &new_inode_num) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Inicializa o novo inode
    struct ext2_inode new_inode = {0};
    new_inode.i_mode = EXT2_S_IFREG | 0644; // Arquivo regular com permissão 644
    new_inode.i_uid = 0;
    new_inode.i_gid = 0;
    new_inode.i_size = 0;
    new_inode.i_links_count = 1;
    new_inode.i_blocks = 0;
    time_t now = time(NULL);
    new_inode.i_atime = new_inode.i_ctime = new_inode.i_mtime = (uint32_t)now;

    // Escreve o novo inode no disco
    if (fs_write_inode(fs, new_inode_num, &new_inode) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Insere a entrada do novo arquivo no diretório pai
    uint8_t buf[EXT2_BLOCK_SIZE];
    uint16_t entry_size = rec_len_needed((uint8_t)strlen(file_name));
    int inserted = 0;

    // Percorre os blocos diretos do diretório pai para inserir a nova entrada
    for (int i = 0; i < 12 && !inserted; ++i)
    {
        uint32_t block = parent_inode.i_block[i];
        if (!block)
        {
            // Se não há bloco, aloca um novo bloco para o diretório
            if (fs_alloc_block(fs, &block) < 0)
            {
                free(full_path);
                free(parent_path);
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
            parent_inode.i_block[i] = block;
            parent_inode.i_size += EXT2_BLOCK_SIZE;
            parent_inode.i_blocks += EXT2_BLOCK_SIZE / 512;
            memset(buf, 0, EXT2_BLOCK_SIZE);

            // Cria a primeira entrada no bloco
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)buf;
            entry->inode = new_inode_num;
            entry->name_len = (uint8_t)strlen(file_name);
            entry->file_type = EXT2_FT_REG_FILE;
            entry->rec_len = EXT2_BLOCK_SIZE;
            memcpy(entry->name, file_name, entry->name_len);

            // Escreve o bloco no disco
            if (fs_write_block(fs, block, buf) < 0)
            {
                free(full_path);
                free(parent_path);
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
            inserted = 1;
            break;
        }

        // Lê o bloco do diretório pai
        if (fs_read_block(fs, block, buf) < 0)
        {
            free(full_path);
            free(parent_path);
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }
        uint32_t pos = 0;
        // Percorre as entradas do diretório procurando espaço para inserir a nova entrada
        while (pos < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + pos);
            if (entry->rec_len == 0)
                break; // Corrupção

            uint16_t ideal = rec_len_needed(entry->name_len);
            uint16_t slack = entry->rec_len - ideal;
            if (slack >= entry_size)
            {
                // Encurta a entrada existente e insere a nova logo após
                entry->rec_len = ideal;
                struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)(buf + pos + ideal);
                new_entry->inode = new_inode_num;
                new_entry->rec_len = slack;
                new_entry->name_len = (uint8_t)strlen(file_name);
                new_entry->file_type = EXT2_FT_REG_FILE;
                memcpy(new_entry->name, file_name, new_entry->name_len);

                // Escreve o bloco atualizado no disco
                if (fs_write_block(fs, block, buf) < 0)
                {
                    free(full_path);
                    free(parent_path);
                    print_error(ERROR_UNKNOWN);
                    return EXIT_FAILURE;
                }
                inserted = 1;
                break;
            }
            pos += entry->rec_len;
        }
    }

    // Se não conseguiu inserir a entrada, retorna erro
    if (!inserted)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Atualiza o inode do diretório pai com as alterações
    fs_write_inode(fs, parent_inode_num, &parent_inode);

    printf("arquivo criado com inode %u.\n", new_inode_num);
    free(full_path);
    free(parent_path);
    return EXIT_SUCCESS;
}
