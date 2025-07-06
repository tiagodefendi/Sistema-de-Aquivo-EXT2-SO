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

    for (int i = 0; i < 12; ++i)
    {
        uint32_t blk = dir_inode->i_block[i];
        if (!blk)
            continue;
        if (fs_read_block(fs, blk, buf) < 0)
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }

        uint32_t off = 0;
        while (off < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *e = (void *)(buf + off);
            if (e->rec_len == 0)
                break;

            if (e->inode != 0 && !(e->name_len == 1 && e->name[0] == '.') && !(e->name_len == 2 && e->name[0] == '.' && e->name[1] == '.'))
            {
                return EXIT_SUCCESS; // encontrou algo além de . e ..
            }
            off += e->rec_len;
        }
    }
    // Se não encontrou nada além de . e ..
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

    for (int i = 0; i < 12; ++i)
    {
        uint32_t blk = parent_inode->i_block[i];
        if (!blk)
            continue;
        if (fs_read_block(fs, blk, buf) < 0)
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }

        uint32_t off = 0;
        struct ext2_dir_entry *prev = NULL;
        while (off < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *e = (void *)(buf + off);
            if (e->rec_len == 0)
                break;

            if (e->inode == target_ino)
            {
                if (prev)
                    prev->rec_len += e->rec_len;
                else
                {
                    e->inode = 0;
                    e->rec_len = EXT2_BLOCK_SIZE;
                }
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
    if (argc != 2)
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    char *full_path = fs_join_path(fs, *cwd, argv[1]);
    if (!full_path)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t dir_ino;
    if (fs_path_resolve(fs, full_path, &dir_ino) < 0)
    {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }

    struct ext2_inode dir_inode;
    if (fs_read_inode(fs, dir_ino, &dir_inode) < 0)
    {
        free(full_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (!ext2_is_dir(&dir_inode))
    {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }

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

    char *parent_path = strdup(full_path);
    char *slash = strrchr(parent_path, '/');
    if (slash && slash != parent_path)
        *slash = '\0';
    else
        strcpy(parent_path, "/");

    uint32_t parent_ino;
    if (fs_path_resolve(fs, parent_path, &parent_ino) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    struct ext2_inode parent_inode;
    if (fs_read_inode(fs, parent_ino, &parent_inode) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (dir_remove_entry_rm(fs, &parent_inode, dir_ino))
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    /* Chamada corrigida para free_inode_blocks */
    if (free_inode_blocks(fs, &dir_inode) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (fs_free_inode(fs, dir_ino) < 0)
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    free(full_path);
    free(parent_path);
    return EXIT_SUCCESS;
}
