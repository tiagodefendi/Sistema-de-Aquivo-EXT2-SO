/*
 * cat.c — comando "cat <filename>"
 *         Exibe conteúdo de um arquivo texto no cwd (por ora somente "/").
 *
 * Limitações aceitas:
 *   • Diretório corrente deve caber em 1 bloco.
 *   • Só trata ponteiros diretos (12) e indireto simples (1).
 *   • Arquivos limitados a ≈ 268 blocos (≈ 268 KiB).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "ext2.h"
#include "utils.h"

#define ROOT_INO 2
#define PTRS_PER_BLOCK (EXT2_BLOCK_SIZE / sizeof(uint32_t))

/* procura por <name> no inode de diretório, devolve nº inode ou 0 */
static uint32_t lookup_file(FILE *img, const struct ext2_super_block *sb,
                            const struct ext2_group_desc *gd,
                            const struct ext2_inode *dir_ino,
                            const char *name)
{
    uint8_t buf[EXT2_BLOCK_SIZE];
    if (read_block(img, dir_ino->i_block[0], buf) != 0)
        return 0;

    uint32_t off = 0;
    while (off < dir_ino->i_size)
    {
        struct ext2_dir_entry *de = (struct ext2_dir_entry *)(buf + off);
        if (de->inode && de->file_type == EXT2_FT_REG_FILE &&
            de->name_len == strlen(name) &&
            strncmp(de->name, name, de->name_len) == 0)
            return de->inode;
        if (de->rec_len == 0)
            break;
        off += de->rec_len;
    }
    return 0;
}

/* lê e despeja nbytes a partir de um bloco apontado */
static int dump_block(FILE *img, uint32_t block_no, uint32_t nbytes)
{
    uint8_t buf[EXT2_BLOCK_SIZE];
    if (read_block(img, block_no, buf) != 0)
        return -1;
    fwrite(buf, 1, nbytes, stdout);
    return 0;
}

int cmd_cat(FILE *img, const char *cwd, const char *filename)
{
    if (!filename)
    {
        fprintf(stderr, "cat: missing filename.\n");
        return -1;
    }
    if (strcmp(cwd, "/") != 0)
    {
        fprintf(stderr, "cat: navegação além da raiz não implementada.\n");
        return -1;
    }

    struct ext2_super_block sb;
    struct ext2_group_desc gd;
    struct ext2_inode dir_ino;

    if (read_superblock(img, &sb) != 0)
        return -1;
    if (read_group_desc(img, &sb, &gd) != 0)
        return -1;
    if (read_inode(img, &sb, &gd, ROOT_INO, &dir_ino) != 0)
        return -1;

    uint32_t file_ino_no = lookup_file(img, &sb, &gd, &dir_ino, filename);
    if (file_ino_no == 0)
    {
        fprintf(stderr, "cat: file not found: %s\n", filename);
        return -1;
    }

    struct ext2_inode file_ino;
    if (read_inode(img, &sb, &gd, file_ino_no, &file_ino) != 0)
    {
        fprintf(stderr, "cat: unable to read inode.\n");
        return -1;
    }
    if ((file_ino.i_mode & 0xF000) != EXT2_S_IFREG)
    {
        fprintf(stderr, "cat: not a regular file.\n");
        return -1;
    }

    uint32_t remaining = file_ino.i_size;

    /* ---- blocos diretos ---- */
    for (int i = 0; i < 12 && remaining; ++i)
    {
        uint32_t to_read = remaining > EXT2_BLOCK_SIZE ? EXT2_BLOCK_SIZE : remaining;
        if (dump_block(img, file_ino.i_block[i], to_read) != 0)
            return -1;
        remaining -= to_read;
    }

    /* ---- indireto simples ---- */
    if (remaining)
    {
        uint32_t *ptrs = (uint32_t *)malloc(EXT2_BLOCK_SIZE);
        if (!ptrs)
            return -1;
        if (read_block(img, file_ino.i_block[12], ptrs) != 0)
        {
            free(ptrs);
            return -1;
        }
        for (uint32_t i = 0; i < PTRS_PER_BLOCK && remaining; ++i)
        {
            if (ptrs[i] == 0)
                break;
            uint32_t to_read = remaining > EXT2_BLOCK_SIZE ? EXT2_BLOCK_SIZE : remaining;
            if (dump_block(img, ptrs[i], to_read) != 0)
            {
                free(ptrs);
                return -1;
            }
            remaining -= to_read;
        }
        free(ptrs);
    }

    /* Arquivos maiores que cobertura acima não são suportados */
    if (remaining)
    {
        fprintf(stderr, "cat: arquivo excede limite suportado.\n");
        return -1;
    }

    return 0;
}
