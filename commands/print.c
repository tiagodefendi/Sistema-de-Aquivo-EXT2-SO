/*
 * print.c — comando "print <subcmd> [args]"
 *           Exibe dados diversos da imagem EXT2.
 *
 * Subcomandos implementados (versão 1):
 *   superblock                  – resumo do superbloco
 *   groups                      – lista descritores do grupo 0..N‑1
 *   inode <nº>                  – dump básico do inode nº (1‑based)
 *   rootdir                     – lista entradas do diretório raiz
 *   dir <nome>                  – lista entradas do diretório <nome> (cwd=/)
 *   inodebitmap [g]             – hex dump do bitmap de inodes (grupo g, default 0)
 *   blockbitmap [g]             – hex dump do bitmap de blocos (grupo g, default 0)
 *   attr <file|dir>             – delega para cmd_attr
 *   block <nº>                  – hex dump de um bloco (tamanho 1 KiB)
 *
 * Simplificações:
 *   • Navegação limitada à raiz; grupos >0 suportados para bitmaps/inode.
 *   • Dump hex simples 16 bytes/linha.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "ext2.h"
#include "utils.h"

extern int cmd_attr(FILE *img, const char *cwd, const char *target);

#define ROOT_INO 2
#define HEX_PER_LINE 16

/* ————— Hex dump helper ————— */
static void hex_dump(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        if (i % HEX_PER_LINE == 0)
            printf("%04zx: ", i);
        printf("%02x ", buf[i]);
        if (i % HEX_PER_LINE == HEX_PER_LINE - 1 || i == len - 1)
            putchar('\n');
    }
}

/* ————— Print superblock ————— */
static int print_super(FILE *img)
{
    struct ext2_super_block sb;
    if (read_superblock(img, &sb) != 0)
        return -1;

    printf("inodes count...............: %u\n", sb.s_inodes_count);
    printf("blocks count...............: %u\n", sb.s_blocks_count);
    printf("reserved blocks count......: %u\n", sb.s_r_blocks_count);
    printf("free blocks count..........: %u\n", sb.s_free_blocks_count);
    printf("free inodes count..........: %u\n", sb.s_free_inodes_count);
    printf("first data block...........: %u\n", sb.s_first_data_block);
    printf("block size.................: %u\n", EXT2_BLOCK_SIZE);
    printf("fragment size..............: %u\n", EXT2_BLOCK_SIZE << sb.s_log_frag_size);
    printf("blocks per group...........: %u\n", sb.s_blocks_per_group);
    printf("fragments per group........: %u\n", sb.s_frags_per_group);
    printf("inodes per group...........: %u\n", sb.s_inodes_per_group);
    printf("mount time.................: %u\n", sb.s_mtime);
    printf("write time.................: %u\n", sb.s_wtime);
    printf("mount count................: %u\n", sb.s_mnt_count);
    printf("max mount count............: %u\n", sb.s_max_mnt_count);
    printf("magic signature............: 0x%04x\n", sb.s_magic);
    printf("file system state..........: %u\n", sb.s_state);
    printf("errors.....................: %u\n", sb.s_errors);
    printf("minor revision level.......: %u\n", sb.s_minor_rev_level);
    printf("time of last check.........: %u\n", sb.s_lastcheck);
    printf("max check interval.........: %u\n", sb.s_checkinterval);
    printf("creator OS.................: %u\n", sb.s_creator_os);
    printf("revision level.............: %u\n", sb.s_rev_level);
    printf("default uid reserved blocks: %u\n", sb.s_def_resuid);
    printf("defautl gid reserved blocks: %u\n", sb.s_def_resgid);
    printf("first non-reserved inode...: %u\n", sb.s_first_ino);
    printf("inode size.................: %u\n", sb.s_inode_size);
    printf("block group number.........: %u\n", sb.s_block_group_nr);
    printf("compatible feature set.....: %x\n", sb.s_feature_compat);
    printf("incompatible feature set...: %x\n", sb.s_feature_incompat);
    printf("read only comp feature set.: %x\n", sb.s_feature_ro_compat);

    printf("volume UUID................: ");
    for (int i = 0; i < 16; ++i)
        printf("%02x", sb.s_uuid[i]);
    printf("\n");

    printf("volume name................: %-16.16s\n", sb.s_volume_name);
    printf("volume last mounted........: %-64.64s\n", sb.s_last_mounted);
    printf("algorithm usage bitmap.....: %u\n", sb.s_algo_bitmap);
    printf("blocks to try preallocate..: %u\n", sb.s_prealloc_blocks);
    printf("blocks preallocate dir.....: %u\n", sb.s_prealloc_dir_blocks);

    printf("journal UUID...............: ");
    for (int i = 0; i < 16; ++i)
        printf("%02x", sb.s_journal_uuid[i]);
    printf("\n");

    printf("journal INum...............: %u\n", sb.s_journal_inum);
    printf("journal Dev................: %u\n", sb.s_journal_dev);
    printf("last orphan................: %u\n", sb.s_last_orphan);

    printf("hash seed..................: ");
    for (int i = 0; i < 4; ++i)
        printf("%08x", sb.s_hash_seed[i]);
    printf("\n");

    printf("default hash version.......: %u\n", sb.s_def_hash_version);
    printf("default mount options......: %u\n", sb.s_default_mount_options);
    printf("first meta.................: %u\n", sb.s_first_meta_bg);
    return 0;
}

/* ————— Print group descriptors ————— */
static int print_groups(FILE *img)
{
    struct ext2_super_block sb;
    if (read_superblock(img, &sb) != 0)
        return -1;

    uint32_t groups = (sb.s_blocks_count + sb.s_blocks_per_group - 1) / sb.s_blocks_per_group;
    for (uint32_t g = 0; g < groups; ++g)
    {
        struct ext2_group_desc gd;
        long off = 1024 + EXT2_BLOCK_SIZE + g * sizeof(struct ext2_group_desc);
        if (fseek(img, off, SEEK_SET) != 0 || fread(&gd, sizeof gd, 1, img) != 1)
            return -1;
        printf("Block Group Descriptor %u:\n", g);
        printf("block bitmap......: %u\n", gd.bg_block_bitmap);
        printf("inode bitmap......: %u\n", gd.bg_inode_bitmap);
        printf("inode table.......: %u\n", gd.bg_inode_table);
        printf("free blocks count.: %u\n", gd.bg_free_blocks_count);
        printf("free inodes count.: %u\n", gd.bg_free_inodes_count);
        printf("used dirs count...: %u\n", gd.bg_used_dirs_count);
        printf("\n");
    }
    return 0;
}

/* ————— Print inode ————— */
static int print_inode(FILE *img, uint32_t ino_no)
{
    struct ext2_super_block sb;
    if (read_superblock(img, &sb) != 0)
        return -1;

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

    printf("File format and access rights...: 0x%04x\n", ino.i_mode);
    printf("User ID.........................: %u\n", ino.i_uid);
    printf("Lower 32-bit file size..........: %u\n", ino.i_size);
    printf("Access time.....................: %u\n", ino.i_atime);
    printf("Creation time...................: %u\n", ino.i_ctime);
    printf("Modification time...............: %u\n", ino.i_mtime);
    printf("Deletion time...................: %u\n", ino.i_dtime);
    printf("Group ID........................: %u\n", ino.i_gid);
    printf("Link count (inode)..............: %u\n", ino.i_links_count);
    printf("512-bytes blocks................: %u\n", ino.i_blocks);
    printf("EXT2 flags......................: %u\n", ino.i_flags);
    printf("Reserved (Linux)................: %u\n", ino.i_osd1);
    for (int i = 0; i < 15; ++i)
        printf("Pointer[%2d].....................: %u\n", i, ino.i_block[i]);
    printf("File version (NFS)..............: %u\n", ino.i_generation);
    printf("Block number ext. attributes....: %u\n", ino.i_file_acl);
    printf("Higher 32-bit file size.........: %u\n", ino.i_dir_acl);
    printf("Location file fragment..........: %u\n", ino.i_faddr);
    return 0;
}

/* ————— Print directory entries for inode ————— */
static int list_dir_inode(FILE *img, const struct ext2_super_block *sb,
                          const struct ext2_group_desc *gd, uint32_t ino_no)
{
    struct ext2_inode ino;
    if (read_inode(img, sb, gd, ino_no, &ino) != 0)
        return -1;
    if (!(ino.i_mode & EXT2_S_IFDIR))
    {
        fprintf(stderr, "not a directory\n");
        return -1;
    }

    uint8_t buf[EXT2_BLOCK_SIZE];
    if (fseek(img, EXT2_BLOCK_OFFSET(ino.i_block[0]), SEEK_SET) != 0 ||
        fread(buf, EXT2_BLOCK_SIZE, 1, img) != 1)
        return -1;

    printf("inode name\n");
    uint32_t off = 0;
    while (off < ino.i_size)
    {
        struct ext2_dir_entry *de = (struct ext2_dir_entry *)(buf + off);
        if (de->inode)
            printf("%5u %*s\n", de->inode, de->name_len, de->name);
        if (!de->rec_len)
            break;
        off += de->rec_len;
    }
    return 0;
}

static int print_rootdir(FILE *img)
{
    struct ext2_super_block sb;
    if (read_superblock(img, &sb) != 0)
        return -1;
    struct ext2_group_desc gd;
    if (read_group_desc(img, &sb, &gd) != 0)
        return -1;
    return list_dir_inode(img, &sb, &gd, ROOT_INO);
}

static int print_dir(FILE *img, const char *name)
{
    /* localiza inode pelo nome na raiz e lista */
    struct ext2_super_block sb;
    if (read_superblock(img, &sb) != 0)
        return -1;
    struct ext2_group_desc gd_root;
    if (read_group_desc(img, &sb, &gd_root) != 0)
        return -1;

    /* acha inode do nome no root */
    struct ext2_inode root_ino;
    if (read_inode(img, &sb, &gd_root, ROOT_INO, &root_ino) != 0)
        return -1;

    uint32_t target_ino = 0;
    uint8_t buf[EXT2_BLOCK_SIZE];
    if (read_block(img, root_ino.i_block[0], buf) != 0)
        return -1;
    uint32_t off = 0;
    size_t nlen = strlen(name);
    while (off < root_ino.i_size)
    {
        struct ext2_dir_entry *de = (struct ext2_dir_entry *)(buf + off);
        if (de->inode && de->name_len == nlen && strncmp(de->name, name, nlen) == 0)
        {
            target_ino = de->inode;
            break;
        }
        if (!de->rec_len)
            break;
        off += de->rec_len;
    }
    if (!target_ino)
    {
        fprintf(stderr, "dir '%s' not found\n", name);
        return -1;
    }

    return list_dir_inode(img, &sb, &gd_root, target_ino);
}

/* ————— Bitmaps ————— */
static int dump_bitmap(FILE *img, uint32_t block)
{
    uint8_t buf[EXT2_BLOCK_SIZE];
    if (read_block(img, block, buf) != 0)
        return -1;
    hex_dump(buf, 64); /* mostra só primeiros 64 bytes p/ clareza */
    return 0;
}

/* ————— Comando principal ————— */
int cmd_print(FILE *img, const char *cwd, int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "print: usage: print <subcmd> [args]\n");
        return -1;
    }
    const char *sub = argv[1];

    if (strcmp(sub, "superblock") == 0)
        return print_super(img);

    if (strcmp(sub, "groups") == 0)
        return print_groups(img);

    if (strcmp(sub, "inode") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "print inode <n>\n");
            return -1;
        }
        return print_inode(img, (uint32_t)atoi(argv[2]));
    }

    if (strcmp(sub, "rootdir") == 0)
        return print_rootdir(img);

    if (strcmp(sub, "dir") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "print dir <name>\n");
            return -1;
        }
        return print_dir(img, argv[2]);
    }

    if (strcmp(sub, "inodebitmap") == 0 || strcmp(sub, "blockbitmap") == 0)
    {
        struct ext2_super_block sb;
        if (read_superblock(img, &sb) != 0)
            return -1;
        uint32_t grp = 0;
        if (argc >= 3)
            grp = (uint32_t)atoi(argv[2]);
        struct ext2_group_desc gd;
        long off = 1024 + EXT2_BLOCK_SIZE + grp * sizeof gd;
        if (fseek(img, off, SEEK_SET) != 0 || fread(&gd, sizeof gd, 1, img) != 1)
            return -1;
        uint32_t blk = strcmp(sub, "inodebitmap") == 0 ? gd.bg_inode_bitmap : gd.bg_block_bitmap;
        return dump_bitmap(img, blk);
    }

    if (strcmp(sub, "attr") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "print attr <file|dir>\n");
            return -1;
        }
        return cmd_attr(img, cwd, argv[2]);
    }

    if (strcmp(sub, "block") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "print block <n>\n");
            return -1;
        }
        uint32_t b = (uint32_t)atoi(argv[2]);
        uint8_t buf[EXT2_BLOCK_SIZE];
        if (read_block(img, b, buf) != 0)
            return -1;
        hex_dump(buf, EXT2_BLOCK_SIZE);
        return 0;
    }

    fprintf(stderr, "print: unknown subcommand '%s'\n", sub);
    return -1;
}
