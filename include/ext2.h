/*
 * Estruturas básicas e definições para manipular imagens EXT2 (versão simplificada para o projeto).
 *
 * Simplificações adotadas:
 *   • Tamanho de bloco fixo em 1024 bytes (EXT2_BLOCK_SIZE).
 *   • Sem suporte a fragmentos ou ponteiros triplamente indiretos (ainda assim mantemos o campo reservando EXT2_N_BLOCKS = 15).
 *   • Máx. de 64 MiB por arquivo e diretórios cabendo em 1 bloco.
 */

#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>

/* ─────────────────────────── Constantes ─────────────────────────── */
#define EXT2_SUPER_MAGIC 0xEF53 /* Assinatura do superbloco */
#define EXT2_BLOCK_SIZE 1024    /* Bloco fixo: 1 KiB        */
#define EXT2_INODE_SIZE 128     /* Tamanho clássico (rev 0) */
#define EXT2_N_BLOCKS 15        /* 12 + 1 + 1 + 1           */
#define EXT2_NAME_LEN 255       /* Máx. nome de arquivo     */

/* Bits de tipo/permissão em i_mode (apenas os usados no projeto) */
#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK 0xA000
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFBLK 0x6000
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFCHR 0x2000
#define EXT2_S_IFIFO 0x1000

#define EXT2_S_IRUSR 0x0100
#define EXT2_S_IWUSR 0x0080
#define EXT2_S_IXUSR 0x0040
#define EXT2_S_IRGRP 0x0020
#define EXT2_S_IWGRP 0x0010
#define EXT2_S_IXGRP 0x0008
#define EXT2_S_IROTH 0x0004
#define EXT2_S_IWOTH 0x0002
#define EXT2_S_IXOTH 0x0001

/* Tipos de entrada em diretório (dir_entry.file_type) */
#define EXT2_FT_UNKNOWN 0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR 2
#define EXT2_FT_CHRDEV 3
#define EXT2_FT_BLKDEV 4
#define EXT2_FT_FIFO 5
#define EXT2_FT_SOCK 6
#define EXT2_FT_SYMLINK 7

/* ───────────────────────── Estruturas on‑disk ───────────────────── */

/* Superbloco – 1024 bytes após o início da imagem */
struct ext2_super_block
{
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t s_uuid[16];
    char s_volume_name[16];
    char s_last_mounted[64];
    uint32_t s_algo_bitmap;
    uint8_t s_prealloc_blocks;
    uint8_t s_prealloc_dir_blocks;
    uint16_t s_alignment;
    uint8_t s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t s_def_hash_version;
    uint8_t s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_options;
    uint32_t s_first_meta_bg;
    uint8_t s_reserved[760];
} __attribute__((packed));

/* Descritor de grupo de blocos */
struct ext2_group_desc
{
    uint32_t bg_block_bitmap;      /* Bloco do bitmap de blocos   */
    uint32_t bg_inode_bitmap;      /* Bloco do bitmap de inodes   */
    uint32_t bg_inode_table;       /* Bloco inicial da tabela ino */
    uint16_t bg_free_blocks_count; /* # blocos livres no grupo    */
    uint16_t bg_free_inodes_count; /* # inodes livres no grupo    */
    uint16_t bg_used_dirs_count;   /* # diretórios no grupo       */
    uint16_t bg_pad;
    uint8_t bg_reserved[12];
} __attribute__((packed));

/* Inode (128 bytes, formato rev 0) */
struct ext2_inode
{
    uint16_t i_mode;                 /* tipo & permissões            */
    uint16_t i_uid;                  /* UID dono                     */
    uint32_t i_size;                 /* tamanho (bytes)              */
    uint32_t i_atime;                /* último acesso (epoch)        */
    uint32_t i_ctime;                /* criação                      */
    uint32_t i_mtime;                /* última modificação           */
    uint32_t i_dtime;                /* deleção (0 se ativo)         */
    uint16_t i_gid;                  /* GID dono                     */
    uint16_t i_links_count;          /* contagem de links            */
    uint32_t i_blocks;               /* blocos de 512 B alocados     */
    uint32_t i_flags;                /* flags ext2                   */
    uint32_t i_osd1;                 /* reservado                    */
    uint32_t i_block[EXT2_N_BLOCKS]; /* ponteiros de dados       */
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl; /* ou high‑size em arquivos >4G */
    uint32_t i_faddr;   /* fragmento (não usado)        */
    uint8_t i_osd2[12]; /* reservado                    */
} __attribute__((packed));

/* Entrada de diretório (máx 255 caracteres) */
struct ext2_dir_entry
{
    uint32_t inode;    /* nº inode do alvo             */
    uint16_t rec_len;  /* deslocamento p/ próxima ent. */
    uint8_t name_len;  /* tamanho do nome              */
    uint8_t file_type; /* EXT2_FT_*                    */
    char name[EXT2_NAME_LEN];
} __attribute__((packed));

/* ──────────────────────── Macros utilitárias ────────────────────── */
#define EXT2_BLOCK_OFFSET(b) ((b) * EXT2_BLOCK_SIZE)
#define EXT2_INODES_PER_BLOCK (EXT2_BLOCK_SIZE / EXT2_INODE_SIZE)

#endif /* EXT2_H */
