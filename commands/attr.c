/*
 * attr.c — comando "attr <file|dir>" com suporte a **indireção simples**.
 *
 * Agora o lookup varre:
 *   • os 12 blocos diretos (i_block[0..11])
 *   • todos os blocos de dados referenciados pelo bloco de ponteiros
 *     simples (i_block[12]), permitindo localizar entradas quando o
 *     diretório ultrapassa 12 KiB.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "ext2.h"
#include "utils.h"

#define ROOT_INO 2
#define PTRS_PER_BLOCK (EXT2_BLOCK_SIZE / sizeof(uint32_t))

/* ───── I/O helpers ───── */
static int read_group_desc_idx(FILE *img, uint32_t idx, struct ext2_group_desc *gd)
{
    long off = 1024 + EXT2_BLOCK_SIZE + idx * sizeof *gd; /* 1 KiB block */
    if (fseek(img, off, SEEK_SET) != 0)
        return -1;
    return fread(gd, sizeof *gd, 1, img) == 1 ? 0 : -1;
}

/* Procura 'name' em blocos diretos e indireto simples de 'dir' */
static uint32_t lookup_inode(FILE *img, const struct ext2_inode *dir,
                             const char *name)
{
    size_t nlen = strlen(name);
    uint8_t buf[EXT2_BLOCK_SIZE];

    /* — 1. Blocos diretos — */
    for (int b = 0; b < 12 && dir->i_block[b]; ++b)
    {
        if (read_block(img, dir->i_block[b], buf) != 0)
            return 0;
        uint32_t off = 0;
        while (off + 8 <= EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *de = (struct ext2_dir_entry *)(buf + off);
            if (de->rec_len < 8)
                break;
            if (de->inode && de->name_len == nlen &&
                strncmp(de->name, name, nlen) == 0)
                return de->inode;
            off += de->rec_len;
        }
    }

    /* — 2. Indireção simples — */
    uint32_t ind_blk = dir->i_block[12];
    if (!ind_blk)
        return 0; /* diretório < 12 KiB */

    uint32_t ptrs[PTRS_PER_BLOCK];
    if (read_block(img, ind_blk, ptrs) != 0)
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
            if (de->inode && de->name_len == nlen &&
                strncmp(de->name, name, nlen) == 0)
                return de->inode;
            off += de->rec_len;
        }
    }
    return 0; /* não achou */
}

/* Tempo → DD/MM/AAAA HH:MM */
static void fmt_time(uint32_t epoch, char *out, size_t sz)
{
    time_t t = (time_t)epoch;
    struct tm *tm = gmtime(&t);
    if (tm)
        strftime(out, sz, "%d/%m/%Y %H:%M", tm);
}

static void fmt_size(uint32_t bytes, char *out, size_t sz)
{
    if (bytes < 1024)
        snprintf(out, sz, "%u B", bytes);
    else if (bytes < 1048576)
        snprintf(out, sz, "%.1f KiB", bytes / 1024.0);
    else
        snprintf(out, sz, "%.1f MiB", bytes / 1048576.0);
}

int cmd_attr(FILE *img, const char *cwd, const char *target)
{
    if (!target)
    {
        fprintf(stderr, "attr: missing operand\n");
        return -1;
    }
    if (strcmp(cwd, "/") != 0)
    {
        fprintf(stderr, "attr: navegação além da raiz não implementada\n");
        return -1;
    }

    /* 1. Superbloco + descritor grupo raiz */
    struct ext2_super_block sb;
    if (read_superblock(img, &sb) != 0)
        return -1;
    struct ext2_group_desc gd_root;
    if (read_group_desc_idx(img, 0, &gd_root) != 0)
        return -1;

    /* 2. Inode raiz */
    struct ext2_inode root_ino;
    if (read_inode(img, &sb, &gd_root, ROOT_INO, &root_ino) != 0)
        return -1;

    /* 3. Localiza inode‑alvo (agora cobre indireção simples) */
    uint32_t ino_no = lookup_inode(img, &root_ino, target);
    if (!ino_no)
    {
        fprintf(stderr, "attr: '%s' not found\n", target);
        return -1;
    }

    /* Grupo que contém o inode */
    uint32_t grp = (ino_no - 1) / sb.s_inodes_per_group;

    /* Descritor do grupo correto */
    struct ext2_group_desc gd;
    long gd_off = 1024 + EXT2_BLOCK_SIZE + grp * sizeof gd;
    if (fseek(img, gd_off, SEEK_SET) != 0 ||
        fread(&gd, sizeof gd, 1, img) != 1)
        return -1;

    /* Índice (0-based) dentro do grupo */
    uint32_t idx_local = (ino_no - 1) % sb.s_inodes_per_group;

    /* Bloco da tabela de inodes + deslocamento */
    uint32_t blk = gd.bg_inode_table + idx_local / EXT2_INODES_PER_BLOCK;
    uint32_t off_in_blk = (idx_local % EXT2_INODES_PER_BLOCK) * EXT2_INODE_SIZE;

    struct ext2_inode ino;
    long off_file = EXT2_BLOCK_OFFSET(blk) + off_in_blk;
    if (fseek(img, off_file, SEEK_SET) != 0 ||
        fread(&ino, sizeof ino, 1, img) != 1)
        return -1;

    /* 5. Formatação de saída */
    char perm[11];
    mode_to_permstr(ino.i_mode, perm);
    char size_str[32];
    fmt_size(ino.i_size, size_str, sizeof size_str);
    char mtime_s[20];
    fmt_time(ino.i_mtime, mtime_s, sizeof mtime_s);

    printf("permissões\tuid\tgid\ttamanho\t\tmodificado em\n");
    printf("%s\t%u\t%u\t%s\t\t%s\n", perm, ino.i_uid, ino.i_gid, size_str, mtime_s);
    return 0;
}
