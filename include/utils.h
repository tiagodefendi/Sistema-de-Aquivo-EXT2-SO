/*
 * utils.h — Funções auxiliares para manipular imagens EXT2
 */

#ifndef UTILS_H
#define UTILS_H

#include "ext2.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Funções de manipulação de imagem */
    int load_image(const char *path);

    /* Funções auxiliares */
    void reset_path();

    /* ──────────────── Interface de alto nível ──────────────── */

    /* Lê o superbloco (offset 1024 B). Retorna 0 em sucesso, -1 erro */
    int read_superblock(FILE *img, struct ext2_super_block *sb);

    /* Lê descritor do grupo 0. Retorna 0 em sucesso, -1 erro */
    int read_group_desc(FILE *img, const struct ext2_super_block *sb,
                        struct ext2_group_desc *gd);

    /* Lê inode de número (1‑based). Retorna 0 em sucesso, -1 erro */
    int read_inode(FILE *img, const struct ext2_super_block *sb,
                   const struct ext2_group_desc *gd, uint32_t inode_no,
                   struct ext2_inode *inode);

    /* Converte i_mode para tipo de arquivo (char 'd', '-', 'l', etc.) */
    char mode_to_filetype(uint16_t mode);

    /* Constrói string de permissões estilo ls (10 chars + NUL) */
    void mode_to_permstr(uint16_t mode, char perm[11]);

    /* Bloco inicial da tabela de inodes do grupo */
    uint32_t inode_table_block(const struct ext2_group_desc *gd);

    /* Debug: imprime resumo do superbloco */
    void print_superblock(const struct ext2_super_block *sb);

    int read_block(FILE *img, uint32_t bno, void *buf);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */
