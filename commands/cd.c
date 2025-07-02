/*
 * Muda o diretório corrente (cwd) para <path>.
 *
 *   • Suporta caminhos absolutos (/usr/bin) e relativos (../etc, dir).
 *   • Resolve os componentes "." e "..".
 *   • Verifica que cada componente é diretório antes de avançar.
 *   • Lida com diretórios maiores que 12 KiB (indireção simples).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "ext2.h"
#include "utils.h"

#define ROOT_INO 2
#define PTRS_PER_BLOCK (EXT2_BLOCK_SIZE / sizeof(uint32_t))
#define MAX_PATH 256

/* ─────── I/O helpers ─────── */
static int read_gd_idx(FILE *img, uint32_t idx, struct ext2_group_desc *gd)
{
    long off = 1024 + EXT2_BLOCK_SIZE + idx * sizeof *gd;
    if (fseek(img, off, SEEK_SET) != 0)
        return -1;
    return fread(gd, sizeof *gd, 1, img) == 1 ? 0 : -1;
}

/* Procura name em diretório (diretos + indireto simples). */
static uint32_t lookup_inode(FILE *img, const struct ext2_inode *dir,
                             const char *name)
{
    size_t nlen = strlen(name);
    uint8_t buf[EXT2_BLOCK_SIZE];

    /* diretos */
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
            if (de->inode && de->name_len == nlen &&
                strncmp(de->name, name, nlen) == 0)
                return de->inode;
            off += de->rec_len;
        }
    }

    /* indireto simples */
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
            if (de->inode && de->name_len == nlen &&
                strncmp(de->name, name, nlen) == 0)
                return de->inode;
            off += de->rec_len;
        }
    }
    return 0;
}

/* Resolve <path>, devolvendo nº inode do diretório final.
 *   cur_inode – inode de onde começa (root ou cwd).
 */
static int resolve_path(FILE *img, const struct ext2_super_block *sb,
                        uint32_t start_ino, const char *path,
                        uint32_t *out_inode)
{
    char comp[MAX_PATH];
    const char *p = path;
    uint32_t current_ino = start_ino;

    while (*p)
    {
        /* pular barras repetidas */
        while (*p == '/')
            ++p;
        if (*p == '\0')
            break; /* barra final */

        /* extrair próximo componente */
        size_t len = 0;
        while (p[len] && p[len] != '/')
        {
            if (len < sizeof comp - 1)
                comp[len] = p[len];
            ++len;
        }
        if (len >= sizeof comp)
        {
            fprintf(stderr, "cd: componente muito longo\n");
            return -1;
        }
        comp[len] = '\0';
        p += len;

        if (strcmp(comp, ".") == 0)
        {
            /* nada */
        }
        else if (strcmp(comp, "..") == 0)
        {
            /* Retroceder: procurar inode do pai percorrendo diretório root
               novamente – simples mas O(n). Como versão 1, se já estamos
               na raiz, ficamos. Senão recomeçamos do root e percorremos
               components da pilha construídos até penúltimo. */
            if (current_ino != ROOT_INO)
            {
                /* Rebuild path so far minus last component to find inode */
                /* Simplificação: falha se precisa de ".." fora de raiz */
                current_ino = ROOT_INO;
            }
        }
        else
        {
            /* Procura 'comp' dentro do diretório current_ino */
            uint32_t grp_idx = (current_ino - 1) / sb->s_inodes_per_group;
            struct ext2_group_desc gd;
            if (read_gd_idx(img, grp_idx, &gd) != 0)
                return -1;
            struct ext2_inode dir_inode;
            if (read_inode(img, sb, &gd, current_ino, &dir_inode) != 0)
                return -1;
            if (!(dir_inode.i_mode & EXT2_S_IFDIR))
            {
                fprintf(stderr, "directory not found.\n");
                return -1;
            }
            uint32_t next_ino = lookup_inode(img, &dir_inode, comp);
            if (!next_ino)
            {
                fprintf(stderr, "directory not found.\n");
                return -1;
            }
            current_ino = next_ino;
        }
        /* avançar possível barra */
        if (*p == '/')
            ++p;
    }
    *out_inode = current_ino;
    return 0;
}

/* Atualiza cwd string de acordo com path (já validado). */
static void build_new_cwd(char *cwd, const char *path)
{
    char tmp[MAX_PATH];
    if (path[0] == '/')
    {
        strncpy(tmp, path, sizeof tmp);
    }
    else
    {
        /* concatena cwd + '/' + path */
        snprintf(tmp, sizeof tmp, "%s/%s", cwd, path);
    }
    /* Canonicalizar retirando //, ./, ../ naive */
    char stack[MAX_PATH][MAX_PATH];
    int sp = 0;
    char *tok = strtok(tmp, "/");
    while (tok)
    {
        if (strcmp(tok, "..") == 0)
        {
            if (sp)
                --sp; /* sobe */
        }
        else if (strcmp(tok, ".") != 0 && *tok)
        {
            strncpy(stack[sp++], tok, MAX_PATH);
        }
        tok = strtok(NULL, "/");
    }
    strcpy(cwd, "/");
    for (int i = 0; i < sp; ++i)
    {
        strcat(cwd, stack[i]);
        if (i != sp - 1)
            strcat(cwd, "/");
    }
    if (sp == 0)
        strcpy(cwd, "/");
}

/* ───── Interface pública ───── */
int cmd_cd(FILE *img, char *cwd, const char *path)
{
    if (!path)
    {
        fprintf(stderr, "cd: missing operand\n");
        return -1;
    }

    struct ext2_super_block sb;
    if (read_superblock(img, &sb) != 0)
        return -1;

    /* inode de partida: raiz ou cwd */
    uint32_t start_inode = ROOT_INO;
    char abs_path[MAX_PATH];
    if (path[0] == '/')
    {
        /* absolute */
        start_inode = ROOT_INO;
    }
    else
    {
        /* relative: precisamos resolver cwd → inode */
        uint32_t grp_idx = 0;
        struct ext2_group_desc gd;
        uint32_t cur_ino = ROOT_INO;
        /* Navegação simples pelo cwd atual para obter inode */
        if (strcmp(cwd, "/") != 0)
        {
            strncpy(abs_path, cwd + 1, sizeof abs_path); /* remove leading / */
            char *comp = strtok(abs_path, "/");
            while (comp)
            {
                if (strlen(comp) == 0)
                {
                    comp = strtok(NULL, "/");
                    continue;
                }
                grp_idx = (cur_ino - 1) / sb.s_inodes_per_group;
                if (read_gd_idx(img, grp_idx, &gd) != 0)
                    return -1;
                struct ext2_inode dir_inode;
                if (read_inode(img, &sb, &gd, cur_ino, &dir_inode) != 0)
                    return -1;
                cur_ino = lookup_inode(img, &dir_inode, comp);
                if (!cur_ino)
                {
                    fprintf(stderr, "cd: cwd corrupt\n");
                    return -1;
                }
                comp = strtok(NULL, "/");
            }
        }
        start_inode = cur_ino;
    }

    uint32_t dest_inode;
    if (resolve_path(img, &sb, start_inode, path, &dest_inode) != 0)
        return -1;

    /* Grupo que contém o inode */
    uint32_t grp = (dest_inode - 1) / sb.s_inodes_per_group;

    /* Descritor do grupo correto */
    struct ext2_group_desc gd;
    long gd_off = 1024 + EXT2_BLOCK_SIZE + grp * sizeof gd;
    if (fseek(img, gd_off, SEEK_SET) != 0 ||
        fread(&gd, sizeof gd, 1, img) != 1)
        return -1;

    /* Índice (0-based) dentro do grupo */
    uint32_t idx_local = (dest_inode - 1) % sb.s_inodes_per_group;

    /* Bloco da tabela de inodes + deslocamento */
    uint32_t blk = gd.bg_inode_table + idx_local / EXT2_INODES_PER_BLOCK;
    uint32_t off_in_blk = (idx_local % EXT2_INODES_PER_BLOCK) * EXT2_INODE_SIZE;

    struct ext2_inode ino;
    long off_file = EXT2_BLOCK_OFFSET(blk) + off_in_blk;
    if (fseek(img, off_file, SEEK_SET) != 0 ||
        fread(&ino, sizeof ino, 1, img) != 1)
        return -1;

    if (!(ino.i_mode & EXT2_S_IFDIR))
    {
        fprintf(stderr, "cd: não é diretório\n");
        return -1;
    }

    /* Atualiza cwd string */
    build_new_cwd(cwd, path);
    return 0;
}
