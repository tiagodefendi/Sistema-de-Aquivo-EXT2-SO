/*
 * ls.c — comando "ls" (v2.1)
 *         Lista o diretório corrente (cwd) em qualquer profundidade.
 *         Agora compatível com C99 — removeu uso de `auto`/lambda.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ext2.h"
#include "utils.h"

#define ROOT_INO 2
#define PTRS_PER_BLOCK (EXT2_BLOCK_SIZE / sizeof(uint32_t))

/* ─── I/O helpers ─── */
static int read_gd_idx(FILE *img, uint32_t idx, struct ext2_group_desc *gd)
{
    long off = 1024 + EXT2_BLOCK_SIZE + idx * sizeof *gd;
    if (fseek(img, off, SEEK_SET) != 0)
        return -1;
    return fread(gd, sizeof *gd, 1, img) == 1 ? 0 : -1;
}

/* Colore e imprime uma entrada */
static void print_entry(const struct ext2_dir_entry *de)
{
    const char *color = "\033[0m";
    if (de->file_type == EXT2_FT_DIR)
        color = "\033[1;34m";
    else if (de->file_type == EXT2_FT_REG_FILE)
        color = "\033[1;32m";
    printf("%s%.*s\033[0m\n", color, de->name_len, de->name);
}

/* lookup_inode e inode_from_cwd iguais à versão anterior ... */
static uint32_t lookup_inode(FILE *img, const struct ext2_inode *dir,
                             const char *name)
{
    size_t nlen = strlen(name);
    uint8_t buf[EXT2_BLOCK_SIZE];
    for (int d = 0; d < 12 && dir->i_block[d]; ++d)
    {
        if (read_block(img, dir->i_block[d], buf) != 0)
            return 0;
        uint32_t off = 0;
        while (off + 8 <= EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *de = (struct ext2_dir_entry *)(buf + off);
            if (de->rec_len < 8)
                break;
            if (de->inode && de->name_len == nlen && strncmp(de->name, name, nlen) == 0)
                return de->inode;
            off += de->rec_len;
        }
    }
    if (!dir->i_block[12])
        return 0;
    uint32_t ptrs[PTRS_PER_BLOCK];
    if (read_block(img, dir->i_block[12], ptrs) != 0)
        return 0;
    for (uint32_t i = 0; i < PTRS_PER_BLOCK && ptrs[i]; ++i)
    {
        if (read_block(img, ptrs[i], buf) != 0)
            return 0;
        uint32_t off = 0;
        while (off + 8 <= EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *de = (struct ext2_dir_entry *)(buf + off);
            if (de->rec_len < 8)
                break;
            if (de->inode && de->name_len == nlen && strncmp(de->name, name, nlen) == 0)
                return de->inode;
            off += de->rec_len;
        }
    }
    return 0;
}

static int inode_from_cwd(FILE *img, const struct ext2_super_block *sb,
                          const char *cwd, uint32_t *out_ino)
{
    if (strcmp(cwd, "/") == 0)
    {
        *out_ino = ROOT_INO;
        return 0;
    }
    uint32_t cur = ROOT_INO;
    char path_copy[256];
    strncpy(path_copy, cwd + 1, sizeof path_copy);
    char *token = strtok(path_copy, "/");
    while (token)
    {
        uint32_t grp = (cur - 1) / sb->s_inodes_per_group;
        struct ext2_group_desc gd;
        if (read_gd_idx(img, grp, &gd) != 0)
            return -1;
        struct ext2_inode dir;
        if (read_inode(img, sb, &gd, cur, &dir) != 0)
            return -1;
        cur = lookup_inode(img, &dir, token);
        if (!cur)
            return -1;
        token = strtok(NULL, "/");
    }
    *out_ino = cur;
    return 0;
}

/* Lista entradas (diretos + indireto simples) */
static int list_directory(FILE *img, const struct ext2_super_block *sb,
                          uint32_t dir_ino_no)
{
    uint32_t grp = (dir_ino_no - 1) / sb->s_inodes_per_group;
    struct ext2_group_desc gd;
    if (read_gd_idx(img, grp, &gd) != 0)
        return -1;
    struct ext2_inode dir;
    if (read_inode(img, sb, &gd, dir_ino_no, &dir) != 0)
        return -1;
    printf("I_MODE: %u\n", dir.i_mode);
    if (!(dir.i_mode & EXT2_S_IFDIR))
    {
        fprintf(stderr, "ls: não é diretório\n");
        return -1;
    }

    uint8_t buf[EXT2_BLOCK_SIZE];
    for (int d = 0; d < 12 && dir.i_block[d]; ++d)
    {
        if (read_block(img, dir.i_block[d], buf) != 0)
            return -1;
        uint32_t off = 0;
        while (off + 8 <= EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *de = (struct ext2_dir_entry *)(buf + off);
            if (de->rec_len < 8)
                break;
            if (de->inode)
                print_entry(de);
            off += de->rec_len;
        }
    }
    if (dir.i_block[12])
    {
        uint32_t ptrs[PTRS_PER_BLOCK];
        if (read_block(img, dir.i_block[12], ptrs) != 0)
            return -1;
        for (uint32_t i = 0; i < PTRS_PER_BLOCK && ptrs[i]; ++i)
        {
            if (read_block(img, ptrs[i], buf) != 0)
                return -1;
            uint32_t off = 0;
            while (off + 8 <= EXT2_BLOCK_SIZE)
            {
                struct ext2_dir_entry *de = (struct ext2_dir_entry *)(buf + off);
                if (de->rec_len < 8)
                    break;
                if (de->inode)
                    print_entry(de);
                off += de->rec_len;
            }
        }
    }
    return 0;
}

/* Interface pública */
int cmd_ls(FILE *img, const char *cwd)
{
    struct ext2_super_block sb;
    if (read_superblock(img, &sb) != 0)
        return -1;
    uint32_t dir_inode;
    if (inode_from_cwd(img, &sb, cwd, &dir_inode) != 0)
    {
        fprintf(stderr, "ls: não foi possível resolver cwd\n");
        return -1;
    }
    return list_directory(img, &sb, dir_inode);
}
