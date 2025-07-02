#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
    uint8_t buf[EXT2_BLOCK_SIZE];

    // Percorre os blocos diretos do diretório
    for (int i = 0; i < 12; ++i)
    {
        uint32_t bloco = dir_inode->i_block[i];
        if (!bloco)
            continue;

        if (fs_read_block(fs, bloco, buf) < 0)
            return -1;

        uint32_t offset = 0;
        while (offset < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + offset);
            if (entry->rec_len == 0)
                break;

            if (entry->inode == tgt_ino)
            {
                memcpy(out, entry, sizeof(struct ext2_dir_entry));
                // Copia o nome separadamente, respeitando o tamanho
                size_t name_len = entry->name_len < 255 ? entry->name_len : 255;
                memcpy(out->name, entry->name, name_len);
                out->name[name_len] = '\0'; // Garante terminação
                return 0;
            }
            offset += entry->rec_len;
        }
    }
    errno = ENOENT;
    return -1;
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
    if (argc != 2)
    {
        fprintf(stderr, "Uso: cd <diretório>\n");
        errno = EINVAL;
        return -1;
    }

    // Resolve o caminho absoluto do diretório alvo
    char *abs_path = fs_join_path(fs, *cwd, argv[1]);
    if (!abs_path)
        return -1;

    uint32_t dir_ino;
    if (fs_path_resolve(fs, abs_path, &dir_ino) < 0)
    {
        fprintf(stderr, "cd: diretório '%s' não encontrado\n", argv[1]);
        free(abs_path);
        return -1;
    }

    struct ext2_inode dir_inode;
    if (fs_read_inode(fs, dir_ino, &dir_inode) < 0)
    {
        fprintf(stderr, "cd: erro ao ler inode do diretório\n");
        free(abs_path);
        return -1;
    }

    if (!ext2_is_dir(&dir_inode))
    {
        fprintf(stderr, "cd: '%s' não é um diretório\n", argv[1]);
        free(abs_path);
        errno = ENOTDIR;
        return -1;
    }

    struct ext2_dir_entry entry;
    if (find_entry_by_ino(fs, &dir_inode, dir_ino, &entry) == 0)
    {
        print_entry(&entry); // Imprime a entrada encontrada
    }

    // Atualiza o diretório corrente
    *cwd = dir_ino;

    free(abs_path);
    return 0;
}
