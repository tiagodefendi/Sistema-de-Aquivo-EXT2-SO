#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "ext2.h"

/**
 * @file    utils.h
 *
 * Definições e protótipos de funções utilitárias para manipulação de imagens EXT2.
 *
 * @authors Artur Bento de Carvalho
 *          Eduardo Riki Matushita
 *          Rafaela Tieri Iwamoto Ferreira
 *          Tiago Defendi da Silva
 *
 * @date    01/07/2025
 */

typedef struct
{
    int fd;                     // Descritor de arquivo da imagem
    struct ext2_super_block sb; // Superbloco do sistema de arquivos
    uint32_t groups_count;      // Número de grupos de blocos
} ext2_fs_t;

/* --------------- Acesso a imagem --------------- */
ext2_fs_t *fs_open(char *img_path);
void fs_close(ext2_fs_t *fs);
off_t fs_block_offset(ext2_fs_t *fs, uint32_t block);
int fs_read_block(ext2_fs_t *fs, uint32_t block, void *buf);
int fs_write_block(ext2_fs_t *fs, uint32_t block, void *buf);

/* --------------- Descritores de grupo --------------- */
int fs_read_group_desc(ext2_fs_t *fs, uint32_t group, struct ext2_group_desc *gd);
int fs_write_group_desc(ext2_fs_t *fs, uint32_t group, struct ext2_group_desc *gd);

/* --------------- Inodes --------------- */
int inode_loc(ext2_fs_t *fs, uint32_t ino, struct ext2_group_desc *gd_out, off_t *off);
int fs_read_inode(ext2_fs_t *fs, uint32_t ino, struct ext2_inode *inode);
int fs_write_inode(ext2_fs_t *fs, uint32_t ino, struct ext2_inode *inode);
int fs_alloc_inode(ext2_fs_t *fs, uint16_t mode, uint32_t *out_ino);
int fs_free_inode(ext2_fs_t *fs, uint32_t ino);
void print_entry(struct ext2_dir_entry *e);

/* --------------- Blocos de dados --------------- */
int fs_alloc_block(ext2_fs_t *fs, uint32_t *out_block);
int fs_free_block(ext2_fs_t *fs, uint32_t block);
int free_inode_blocks(ext2_fs_t *fs, struct ext2_inode *inode);

/* --------------- Diretórios --------------- */
typedef int (*dir_iter_cb)(struct ext2_dir_entry *entry, void *user);
int fs_iterate_dir(ext2_fs_t *fs, struct ext2_inode *dir_inode, dir_iter_cb cb, void *user);
int fs_find_in_dir(ext2_fs_t *fs, struct ext2_inode *dir_inode, char *name, uint32_t *out_ino);
uint16_t rec_len_needed(uint8_t name_len);

/* --------------- Caminhos --------------- */
int fs_path_resolve(ext2_fs_t *fs, char *path, uint32_t *ino);
char *fs_get_path(ext2_fs_t *fs, uint32_t dir_ino);
char *fs_join_path(ext2_fs_t *fs, uint32_t cwd, const char *rel);

/* --------------- Sincronização --------------- */
int fs_sync_super(ext2_fs_t *fs);

/* --------------- Nomes --------------- */
int name_exists(ext2_fs_t *fs, struct ext2_inode *dir_inode, char *name);

/* --------------- Conveniências --------------- */
static inline int ext2_is_dir(struct ext2_inode *inode) // Verifica se o inode é um diretório
{
    return (inode->i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR;
}

static inline int ext2_is_reg(struct ext2_inode *inode) // Verifica se o inode é um arquivo regular
{
    return (inode->i_mode & EXT2_S_IFREG) == EXT2_S_IFREG;
}

#endif