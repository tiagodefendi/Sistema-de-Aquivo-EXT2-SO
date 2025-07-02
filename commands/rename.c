#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "commands.h"

/**
 * @brief Renomeia uma entrada de diretório no sistema de arquivos EXT2.
 *
 * Esta função procura uma entrada de diretório com o inode especificado e renomeia-a
 * para o novo nome fornecido. Se a entrada não for encontrada, retorna -1.
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param blk Bloco onde a entrada de diretório está localizada.
 * @param target_ino Número do inode da entrada a ser renomeada.
 * @param newname Novo nome para a entrada de diretório.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
static int rename_entry_block(ext2_fs_t *fs, uint32_t blk, uint32_t target_ino, char *newname)
{
    uint8_t buf[EXT2_BLOCK_SIZE];
    if (fs_read_block(fs, blk, buf) < 0) // Lê o bloco do diretório
        return -1;

    uint32_t pos = 0;
    while (pos < EXT2_BLOCK_SIZE)
    {
        struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + pos); // Obtém a entrada de diretório atual
        if (entry->rec_len == 0)
            break;

        if (entry->inode == target_ino)
        {
            uint8_t newname_len = (uint8_t)strlen(newname);      // Comprimento do novo nome
            uint16_t required_len = rec_len_needed(newname_len); // Calcula o comprimento necessário para a nova entrada

            if (required_len > entry->rec_len)
            {
                errno = ENOSPC;
                return -1;
            }

            entry->name_len = newname_len;
            memcpy(entry->name, newname, newname_len); // Copia o novo nome para a entrada

            // Preenche com zeros o restante do nome antigo, se necessário
            if (newname_len < entry->rec_len - 8)
                memset(entry->name + newname_len, 0, entry->rec_len - 8 - newname_len); // Preenche com zeros após o novo nome

            // Salva o bloco modificado
            return fs_write_block(fs, blk, buf);
        }

        pos += entry->rec_len; // Avança para a próxima entrada
    }

    errno = ENOENT;
    return -1;
}

/**
 * @brief Comando para renomear um arquivo no sistema de arquivos EXT2.
 *
 * Este comando renomeia um arquivo especificado por seu caminho para um novo nome.
 *
 * @param argc Número de argumentos da linha de comando.
 * @param argv Lista de argumentos da linha de comando.
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o inode do diretório de trabalho atual.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_rename(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 3)
    {
        fprintf(stderr, "Uso: rename <arquivo> <novo_nome>\n");
        errno = EINVAL;
        return -1;
    }

    char *old_path = argv[1];
    char *new_name = argv[2];

    if (strlen(new_name) > 255)
    {
        fprintf(stderr, "novo_nome muito longo\n");
        errno = ENAMETOOLONG;
        return -1;
    }

    // Resolve o caminho completo do arquivo antigo
    char *old_full_path = fs_join_path(fs, *cwd, old_path);
    if (!old_full_path)
        return -1;

    uint32_t old_inode;
    if (fs_path_resolve(fs, old_full_path, &old_inode) < 0) // Resolve o inode do arquivo antigo
    {
        free(old_full_path);
        return -1;
    }

    // Descobre o diretório pai do arquivo
    char *parent_path;
    char *dup_path = strdup(old_full_path);
    char *last_slash = strrchr(dup_path, '/');
    if (!last_slash)
        parent_path = fs_get_path(fs, *cwd);
    else if (last_slash == dup_path)
        parent_path = strdup("/");
    else
    {
        *last_slash = '\0';
        parent_path = strdup(dup_path);
    }

    uint32_t parent_inode_num;
    if (fs_path_resolve(fs, parent_path, &parent_inode_num) < 0) // Resolve o inode do diretório pai
    {
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        return -1;
    }

    struct ext2_inode parent_inode;
    if (fs_read_inode(fs, parent_inode_num, &parent_inode) < 0) // Lê o inode do diretório pai
    {
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        return -1;
    }

    int already_exists = name_exists(fs, &parent_inode, new_name); // Verifica se já existe uma entrada com o novo nome
    if (already_exists < 0)
    {
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        return -1;
    }
    if (already_exists == 1)
    {
        fprintf(stderr, "%s: já existe\n", new_name);
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        errno = EEXIST;
        return -1;
    }

    // Procura e renomeia a entrada no diretório pai
    int renamed = 0;
    for (int i = 0; i < 12; ++i)
    {
        uint32_t block = parent_inode.i_block[i];
        if (!block)
            continue;
        if (rename_entry_block(fs, block, old_inode, new_name) == 0) // Renomeia a entrada no bloco
        {
            renamed = 1;
            break;
        }
        if (errno != ENOENT)
        {
            free(old_full_path);
            free(dup_path);
            free(parent_path);
            return -1;
        }
    }
    if (!renamed)
    {
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        errno = ENOENT;
        return -1;
    }

    free(old_full_path);
    free(dup_path);
    free(parent_path);
    return 0;
}
