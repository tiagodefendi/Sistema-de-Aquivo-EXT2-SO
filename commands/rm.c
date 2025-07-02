#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "commands.h"

/** * @brief Remove uma entrada de diretório.
 *
 * Esta função percorre os blocos diretos do diretório e remove a entrada
 * correspondente ao inode especificado, definindo seu inode como 0.
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode Ponteiro para o inode do diretório onde a entrada será removida.
 * @param target_ino Número do inode da entrada a ser removida.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int dir_remove_entry_rm(ext2_fs_t *fs, struct ext2_inode *dir_inode, uint32_t target_ino)
{
    uint8_t buf[EXT2_BLOCK_SIZE];

    // Percorre os blocos diretos do diretório
    for (int i = 0; i < 12; ++i)
    {
        uint32_t bloco = dir_inode->i_block[i];
        if (!bloco)
            continue;

        // Lê o bloco do diretório
        if (fs_read_block(fs, bloco, buf) < 0)
            return -1;

        uint32_t offset = 0;
        int encontrou = 0;

        // Percorre as entradas de diretório dentro do bloco
        while (offset < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + offset);
            if (entry->rec_len == 0)
                break;

            // Se encontrou a entrada com o inode alvo, remove
            if (entry->inode == target_ino)
            {
                entry->inode = 0;
                encontrou = 1;
                break;
            }
            offset += entry->rec_len;
        }

        // Se modificou, grava o bloco de volta e retorna sucesso
        if (encontrou)
        {
            if (fs_write_block(fs, bloco, buf) < 0)
                return -1;
            return 0;
        }
    }

    // Não encontrou a entrada
    errno = ENOENT;
    return -1;
}

/** @brief Remove um arquivo do sistema de arquivos EXT2.
 *
 * Esta função remove um arquivo especificado pelo caminho relativo, liberando
 * os blocos alocados e o inode associado.
 *
 * @param argc Número de argumentos da linha de comando.
 * @param argv Array de argumentos da linha de comando.
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o inode do diretório atual.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_rm(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: rm <arquivo>\n");
        errno = EINVAL;
        return -1;
    }

    // Resolve o caminho absoluto do arquivo a ser removido
    char *full_path = fs_join_path(fs, *cwd, argv[1]);
    if (!full_path)
        return -1;

    // Resolve o inode do arquivo
    uint32_t file_ino;
    if (fs_path_resolve(fs, full_path, &file_ino) < 0)
    {
        free(full_path);
        return -1;
    }

    struct ext2_inode file_inode;
    if (fs_read_inode(fs, file_ino, &file_inode) < 0)
    {
        free(full_path);
        return -1;
    }

    // Não permite remover diretórios com rm
    if (ext2_is_dir(&file_inode))
    {
        fprintf(stderr, "%s: é um diretório (use rmdir)\n", argv[1]);
        free(full_path);
        errno = EISDIR;
        return -1;
    }

    // Descobre o caminho do diretório pai
    char *parent_path = strdup(full_path);
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash && last_slash != parent_path)
        *last_slash = '\0';
    else
        strcpy(parent_path, "/");

    // Resolve o inode do diretório pai
    uint32_t parent_ino;
    if (fs_path_resolve(fs, parent_path, &parent_ino) < 0)
    {
        free(full_path);
        free(parent_path);
        return -1;
    }

    struct ext2_inode parent_inode;
    if (fs_read_inode(fs, parent_ino, &parent_inode) < 0)
    {
        free(full_path);
        free(parent_path);
        return -1;
    }

    // Remove a entrada do diretório pai
    if (dir_remove_entry_rm(fs, &parent_inode, file_ino) < 0)
    {
        free(full_path);
        free(parent_path);
        return -1;
    }

    // Libera blocos e inode do arquivo
    free_inode_blocks(fs, &file_inode);
    fs_free_inode(fs, file_ino);

    free(full_path);
    free(parent_path);
    return 0;
}
