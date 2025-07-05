#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>

/**
 * @file    ext2.h
 *
 * Definições e estruturas para manipulação de imagens EXT2.
 *
 * @authors Artur Bento de Carvalho
 *          Eduardo Riki Matushita
 *          Rafaela Tieri Iwamoto Ferreira
 *          Tiago Defendi da Silva
 *
 * @date    01/07/2025
 */

#define EXT2_SUPER_MAGIC 0xEF53 // Assinatura do superbloco
#define EXT2_BLOCK_SIZE 1024    // Bloco fixo: 1 KiB
#define PTRS_PER_BLOCK 256      // 1024 / 4
#define EXT2_N_BLOCKS 15        // 12 + 1 + 1 + 1
#define EXT2_NAME_LEN 255       // Tamanho máximo de nome de arquivo

/* --------------- Valores de modo de arquivo (i_mode) --------------- */

// Formato de arquivo
#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK 0xA000
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFBLK 0x6000
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFCHR 0x2000
#define EXT2_S_IFIFO 0x1000
// Direitos de acesso
#define EXT2_S_IRUSR 0x0100
#define EXT2_S_IWUSR 0x0080
#define EXT2_S_IXUSR 0x0040
#define EXT2_S_IRGRP 0x0020
#define EXT2_S_IWGRP 0x0010
#define EXT2_S_IXGRP 0x0008
#define EXT2_S_IROTH 0x0004
#define EXT2_S_IWOTH 0x0002
#define EXT2_S_IXOTH 0x0001

// Tipos de entrada em diretório
#define EXT2_FT_UNKNOWN 0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR 2
#define EXT2_FT_CHRDEV 3
#define EXT2_FT_BLKDEV 4
#define EXT2_FT_FIFO 5
#define EXT2_FT_SOCK 6
#define EXT2_FT_SYMLINK 7

// Inodes reservados
#define EXT2_BAD_INO 1
#define EXT2_ROOT_INO 2

// Número máximo de blocos diretos em um inode
#define BIT_BYTE(b) ((b) >> 3)
#define BIT_MASK(b) (1U << ((b) & 7))

/* --------------- Estruturas --------------- */

struct ext2_super_block // Superbloco
{
    /* 0 */ uint32_t s_inodes_count;       // Número total de inodes
    /* 4 */ uint32_t s_blocks_count;       // Número total de blocos
    /* 8 */ uint32_t s_r_blocks_count;     // Blocos reservados para root
    /* 12 */ uint32_t s_free_blocks_count; // Blocos livres
    /* 16 */ uint32_t s_free_inodes_count; // Inodes livres
    /* 20 */ uint32_t s_first_data_block;  // Primeiro bloco de dados
    /* 24 */ uint32_t s_log_block_size;    // log2(tamanho do bloco) - 10
    /* 28 */ uint32_t s_log_frag_size;     // log2(tamanho do fragmento) - 10
    /* 32 */ uint32_t s_blocks_per_group;  // Blocos por grupo
    /* 36 */ uint32_t s_frags_per_group;   // Fragmentos por grupo
    /* 40 */ uint32_t s_inodes_per_group;  // Inodes por grupo
    /* 44 */ uint32_t s_mtime;             // Hora do último mount
    /* 48 */ uint32_t s_wtime;             // Hora do último write
    /* 52 */ uint16_t s_mnt_count;         // Contador de montagens desde o último fsck
    /* 54 */ uint16_t s_max_mnt_count;     // Máximo de montagens antes do fsck
    /* 56 */ uint16_t s_magic;             // Número mágico (0xEF53)
    /* 58 */ uint16_t s_state;             // Estado do sistema de arquivos
    /* 60 */ uint16_t s_errors;            // Comportamento em caso de erro
    /* 62 */ uint16_t s_minor_rev_level;   // Versão menor
    /* 64 */ uint32_t s_lastcheck;         // Última verificação (fsck)
    /* 68 */ uint32_t s_checkinterval;     // Intervalo entre verificações
    /* 72 */ uint32_t s_creator_os;        // Sistema operacional criador
    /* 76 */ uint32_t s_rev_level;         // Nível de revisão
    /* 80 */ uint16_t s_def_resuid;        // UID padrão para arquivos reservados
    /* 82 */ uint16_t s_def_resgid;        // GID padrão para arquivos reservados

    // -- EXT2_DYNAMIC_REV Specific --
    /* 84 */ uint32_t s_first_ino;          // Primeiro inode utilizável
    /* 88 */ uint16_t s_inode_size;         // Tamanho do inode (em bytes)
    /* 90 */ uint16_t s_block_group_nr;     // Número do grupo de blocos para este superbloco
    /* 92 */ uint32_t s_feature_compat;     // Recursos compatíveis
    /* 96 */ uint32_t s_feature_incompat;   // Recursos incompatíveis
    /* 100 */ uint32_t s_feature_ro_compat; // Recursos somente-leitura compatíveis
    /* 104 */ uint8_t s_uuid[16];           // UUID do volume
    /* 120 */ char s_volume_name[16];       // Nome do volume
    /* 136 */ char s_last_mounted[64];      // Caminho onde foi montado pela última vez
    /* 200 */ uint32_t s_algo_bitmap;       // Algoritmo de bitmap usado

    // -- Performance hints --
    /* 204 */ uint8_t s_prealloc_blocks;     // Blocos prealocados para arquivos
    /* 205 */ uint8_t s_prealloc_dir_blocks; // Blocos prealocados para diretórios
    /* 206 */ uint16_t s_alignment;          // Alinhamento de bloco (não oficial, usado em algumas distros)

    // -- Journaling support --
    /* 208 */ uint8_t s_journal_uuid[16]; // UUID do dispositivo de journal
    /* 224 */ uint32_t s_journal_inum;    // Inode do journal
    /* 228 */ uint32_t s_journal_dev;     // Número do dispositivo do journal
    /* 232 */ uint32_t s_last_orphan;     // Início da lista de inodes órfãos

    // -- Directory Indexing Support --
    /* 236 */ uint32_t s_hash_seed[4];    // Sementes para hash de diretório
    /* 252 */ uint8_t s_def_hash_version; // Versão padrão do hash
    /* 253 */ uint8_t s_pad[3];           // Preenchimento / reservado

    // -- Outras opções --
    /* 256 */ uint32_t s_default_mount_options; // Opções padrão de montagem
    /* 260 */ uint32_t s_first_meta_bg;         // Primeiro grupo de blocos de metadados

    // -- Reservado para futuras versões --
    /* 264 */ uint8_t s_reserved[760]; // Reservado
} __attribute__((packed));

struct ext2_group_desc // Descritor de grupo de blocos
{
    /*  0 */ uint32_t bg_block_bitmap;      // Bloco onde começa o mapa de blocos
    /*  4 */ uint32_t bg_inode_bitmap;      // Bloco onde começa o mapa de inodes
    /*  8 */ uint32_t bg_inode_table;       // Bloco onde começa a tabela de inodes
    /* 12 */ uint16_t bg_free_blocks_count; // Número de blocos livres no grupo
    /* 14 */ uint16_t bg_free_inodes_count; // Número de inodes livres no grupo
    /* 16 */ uint16_t bg_used_dirs_count;   // Número de diretórios usados no grupo
    /* 18 */ uint16_t bg_pad;               // Preenchimento para alinhamento
    /* 20 */ uint8_t bg_reserved[12];       // Reservado para uso futuro
} __attribute__((packed));

struct ext2_inode // Inode (estrutura de arquivo)
{
    /*   0 */ uint16_t i_mode;        // Tipo e permissões do arquivo
    /*   2 */ uint16_t i_uid;         // UID do proprietário
    /*   4 */ uint32_t i_size;        // Tamanho do arquivo em bytes
    /*   8 */ uint32_t i_atime;       // Hora de último acesso
    /*  12 */ uint32_t i_ctime;       // Hora de criação
    /*  16 */ uint32_t i_mtime;       // Hora de modificação
    /*  20 */ uint32_t i_dtime;       // Hora de exclusão
    /*  24 */ uint16_t i_gid;         // GID do grupo
    /*  26 */ uint16_t i_links_count; // Número de links (entradas de diretório)
    /*  28 */ uint32_t i_blocks;      // Número de blocos alocados (em setores de 512 bytes)
    /*  32 */ uint32_t i_flags;       // Flags de comportamento
    /*  36 */ uint32_t i_osd1;        // Campo específico do sistema operacional
    /*  40 */ uint32_t i_block[15];   // Ponteiros para blocos de dados (diretos, indiretos, etc.)
    /* 100 */ uint32_t i_generation;  // Número da geração (para NFS)
    /* 104 */ uint32_t i_file_acl;    // Bloco de ACL do arquivo (ou bloco de dados grande no ext2)
    /* 108 */ uint32_t i_dir_acl;     // Tamanho alto do arquivo se for diretório (EXT2_DYNAMIC_REV)
    /* 112 */ uint32_t i_faddr;       // Endereço do fragmento (não usado no ext2)
    /* 116 */ uint8_t i_osd2[12];     // Campos adicionais específicos do sistema operacional
} __attribute__((packed));

struct ext2_dir_entry // Entrada de diretório
{
    /*   0 */ uint32_t inode;           // Número do inode apontado pela entrada
    /*   4 */ uint16_t rec_len;         // Comprimento total desta entrada de diretório
    /*   6 */ uint8_t name_len;         // Comprimento do nome do arquivo
    /*   7 */ uint8_t file_type;        // Tipo de arquivo (apenas se EXT2_FEATURE_INCOMPAT_FILETYPE)
    /*   8 */ char name[EXT2_NAME_LEN]; // Nome do arquivo (não nulo, não terminado com '\0')
} __attribute__((packed));

/* --------------- Macros utilitárias --------------- */

#define EXT2_SUPER_OFFSET 1024

#endif
