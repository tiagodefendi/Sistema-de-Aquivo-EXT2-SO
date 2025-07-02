#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "commands.h"

/**
 * @brief Verifica se um diretório está vazio.
 *
 * Esta função percorre os blocos diretos do diretório e verifica se há entradas
 * além de "." e "..". Se encontrar qualquer outra entrada, retorna 1 (não vazio).
 * Se não encontrar nenhuma entrada, retorna 0 (vazio).
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode Ponteiro para o inode do diretório a ser verificado.
 *
 * @return Retorna 0 se o diretório estiver vazio, 1 se não estiver vazio, ou -1 em caso de erro.
 */
int dir_is_empty(ext2_fs_t *fs, struct ext2_inode *dir_inode)
{
    uint8_t buf[EXT2_BLOCK_SIZE];
    for (int i = 0; i < 12; ++i)
    {
        uint32_t blk = dir_inode->i_block[i];
        if (!blk)
            continue;
        if (fs_read_block(fs, blk, buf) < 0)
            return -1;

        uint32_t pos = 0;
        while (pos < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + pos);
            if (entry->rec_len == 0)
                break;

            if (entry->inode != 0)
            {
                // Ignora "." e ".."
                if (!(entry->name_len == 1 && entry->name[0] == '.') &&
                    !(entry->name_len == 2 && entry->name[0] == '.' && entry->name[1] == '.'))
                {
                    errno = ENOTEMPTY;
                    return 1;
                }
            }
            pos += entry->rec_len;
        }
    }
    return 0;
}

/**
 * @brief Remove uma entrada de diretório.
 *
 * Esta função percorre os blocos diretos do diretório e remove a entrada
 * correspondente ao inode especificado, definindo seu inode como 0.
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode Ponteiro para o inode do diretório onde a entrada será removida.
 * @param dir_ino Número do inode do diretório pai.
 * @param tgt_ino Número do inode da entrada a ser removida.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int dir_remove_entry_rmdir(ext2_fs_t *fs, struct ext2_inode *dir_inode, uint32_t dir_ino, uint32_t tgt_ino)
{
    uint8_t buf[EXT2_BLOCK_SIZE];
    for (int i = 0; i < 12; ++i)
    {
        uint32_t blk = dir_inode->i_block[i];
        if (!blk)
            continue;
        if (fs_read_block(fs, blk, buf) < 0)
            return -1;

        uint32_t pos = 0;
        struct ext2_dir_entry *prev = NULL;
        while (pos < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + pos);
            if (entry->rec_len == 0)
                break;

            if (entry->inode == tgt_ino)
            {
                if (prev)
                {
                    // Remove entry by merging rec_len with previous entry
                    prev->rec_len += entry->rec_len;
                }
                else
                {
                    // First entry: just clear inode
                    entry->inode = 0;
                }

                if (fs_write_block(fs, blk, buf) < 0)
                    return -1;

                // Atualiza o link count do diretório
                dir_inode->i_links_count--;
                return fs_write_inode(fs, dir_ino, dir_inode);
            }

            prev = entry;
            pos += entry->rec_len;
        }
    }
    errno = ENOENT;
    return -1;
}

/**
 * @brief Remove um diretório do sistema de arquivos EXT2.
 *
 * Esta função remove um diretório especificado pelo caminho relativo, liberando
 * os blocos alocados e o inode associado. O diretório deve estar vazio.
 *
 * @param argc Número de argumentos da linha de comando.
 * @param argv Array de argumentos da linha de comando.
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o inode do diretório atual.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_rmdir(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: rmdir <diretório>\n");
        errno = EINVAL;
        return -1;
    }

    // Resolve o caminho absoluto do diretório a ser removido
    char *target_path = fs_join_path(fs, *cwd, argv[1]);
    if (!target_path)
        return -1;

    // Obtém o inode do diretório alvo
    uint32_t target_ino;
    if (fs_path_resolve(fs, target_path, &target_ino) < 0)
    {
        free(target_path);
        return -1;
    }

    struct ext2_inode target_inode;
    if (fs_read_inode(fs, target_ino, &target_inode) < 0)
    {
        free(target_path);
        return -1;
    }

    // Verifica se é realmente um diretório
    if (!ext2_is_dir(&target_inode))
    {
        fprintf(stderr, "%s: não é diretório\n", argv[1]);
        free(target_path);
        errno = ENOTDIR;
        return -1;
    }

    // Verifica se o diretório está vazio
    int empty = dir_is_empty(fs, &target_inode);
    if (empty == -1)
    {
        free(target_path);
        return -1;
    }
    if (empty == 1)
    {
        fprintf(stderr, "%s: diretório não vazio\n", argv[1]);
        free(target_path);
        return -1;
    }

    // Descobre o caminho do diretório pai
    char *parent_path = strdup(target_path);
    char *slash = strrchr(parent_path, '/');
    if (slash && slash != parent_path)
        *slash = '\0';
    else
        strcpy(parent_path, "/");

    // Obtém o inode do diretório pai
    uint32_t parent_ino;
    if (fs_path_resolve(fs, parent_path, &parent_ino) < 0)
    {
        free(target_path);
        free(parent_path);
        return -1;
    }

    struct ext2_inode parent_inode;
    if (fs_read_inode(fs, parent_ino, &parent_inode) < 0)
    {
        free(target_path);
        free(parent_path);
        return -1;
    }

    // Remove a entrada do diretório pai
    if (dir_remove_entry_rmdir(fs, &parent_inode, parent_ino, target_ino) < 0)
    {
        free(target_path);
        free(parent_path);
        return -1;
    }

    // Libera blocos e inode do diretório removido
    free_inode_blocks(fs, &target_inode);
    fs_free_inode(fs, target_ino);

    free(target_path);
    free(parent_path);
    return 0;
}
