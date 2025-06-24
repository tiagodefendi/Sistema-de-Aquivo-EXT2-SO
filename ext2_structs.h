#ifndef EXT2_STRUCTS_H
#define EXT2_STRUCTS_H

#include <stdint.h>

typedef struct {
    // Superblock Structure
    uint32_t inodes_count;           // Número total de inodes no sistema de arquivos
    uint32_t blocks_count;           // Número total de blocos no sistema de arquivos
    uint32_t reserved_blocks_count;  // Número de blocos reservados para o superusuário
    uint32_t free_blocks_count;      // Número de blocos livres
    uint32_t free_inodes_count;      // Número de inodes livres
    uint32_t first_data_block;       // Número do primeiro bloco de dados
    uint32_t log_block_size;         // log2(tamanho do bloco) - 10
    uint32_t log_frag_size;          // log2(tamanho do fragmento) - 10
    uint32_t blocks_per_group;       // Número de blocos por grupo
    uint32_t frags_per_group;        // Número de fragmentos por grupo
    uint32_t inodes_per_group;       // Número de inodes por grupo
    uint32_t mtime;                  // Última montagem (timestamp UNIX)
    uint32_t wtime;                  // Última escrita (timestamp UNIX)
    uint16_t mount_count;            // Contador de montagens desde o último check
    uint16_t max_mount_count;        // Máximo de montagens antes de forçar fsck
    uint16_t magic;                  // Número mágico do EXT2 (0xEF53)
    uint16_t state;                  // Estado do sistema de arquivos (montado limpo/sujo)
    uint16_t errors;                 // Comportamento em caso de erro
    uint16_t minor_rev_level;        // Revisão menor
    uint32_t lastcheck;              // Última verificação (timestamp UNIX)
    uint32_t checkinterval;          // Intervalo máximo entre verificações
    uint32_t creator_os;             // SO que criou o sistema de arquivos
    uint32_t rev_level;              // Revisão do sistema de arquivos
    uint16_t def_resuid;             // UID reservado padrão para blocos reservados
    uint16_t def_resgid;             // GID reservado padrão para blocos reservados
    // -- EXT2_DYNAMIC_REV Specific --
    uint32_t first_ino;              // Primeiro inode utilizável para arquivos normais
    uint16_t inode_size;             // Tamanho de cada inode
    uint16_t block_group_nr;         // Número do grupo de blocos deste superbloco
    uint32_t feature_compat;         // Recursos compatíveis
    uint32_t feature_incompat;       // Recursos incompatíveis
    uint32_t feature_ro_compat;      // Recursos somente leitura compatíveis
    uint8_t uuid[16];                // UUID do sistema de arquivos
    char volume_name[16];            // Nome do volume
    char last_mounted[64];           // Caminho do último ponto de montagem
    uint32_t algo_bitmap;            // Algoritmo de bitmap para alocação de blocos
    // -- Performance Hints --
    uint8_t prealloc_blocks;         // Blocos pré-alocados para arquivos
    uint8_t prealloc_dir_blocks;     // Blocos pré-alocados para diretórios
    uint16_t padding;                // Preenchimento/alinhamento
    // -- Journaling Support --
    uint8_t journal_uuid[16];        // UUID do journal
    uint32_t journal_inum;           // Número do inode do journal
    uint32_t journal_dev;            // Número do dispositivo do journal
    uint32_t last_orphan;            // Início da lista de inodes órfãos
    // -- Directory Indexing Support --
    uint32_t hash_seed[4];           // Sementes para hash de diretórios
    uint8_t def_hash_version;        // Versão padrão do algoritmo de hash
    uint8_t reserved_char_pad;       // Preenchimento/alinhamento
    // Other reserved fields
    uint8_t reserved[760];           // Reservado para uso futuro
} __attribute__((packed)) ext2_super_block;

typedef struct {
    // Block Group Descriptor Structure
    uint32_t block_bitmap;        // Número do bloco do bitmap de blocos (indica blocos livres/ocupados)
    uint32_t inode_bitmap;        // Número do bloco do bitmap de inodes (indica inodes livres/ocupados)
    uint32_t inode_table;         // Número do bloco inicial da tabela de inodes deste grupo
    uint16_t free_blocks_count;   // Quantidade de blocos livres neste grupo
    uint16_t free_inodes_count;   // Quantidade de inodes livres neste grupo
    uint16_t used_dirs_count;     // Quantidade de diretórios usados neste grupo
    uint16_t pad;                 // Preenchimento/alinhamento (não utilizado)
    uint8_t reserved[12];         // Reservado para uso futuro
} __attribute__((packed)) ext2_group_desc;

typedef struct {
    // Inode Structure
    uint16_t mode;         // Tipo e permissões do arquivo (ex: diretório, arquivo regular, etc)
    uint16_t uid;          // ID do usuário proprietário do arquivo
    uint32_t size;         // Tamanho do arquivo em bytes (ou tamanho do diretório)
    uint32_t atime;        // Último acesso ao arquivo (timestamp UNIX)
    uint32_t ctime;        // Última alteração do inode (timestamp UNIX)
    uint32_t mtime;        // Última modificação do arquivo (timestamp UNIX)
    uint32_t dtime;        // Hora da exclusão do arquivo (timestamp UNIX)
    uint16_t gid;          // ID do grupo proprietário do arquivo
    uint16_t links_count;  // Número de links (quantas entradas de diretório apontam para este inode)
    uint32_t blocks;       // Número de blocos de 512 bytes alocados para este arquivo
    uint32_t flags;        // Flags de status e comportamento do arquivo (ex: imutável, append-only)
    uint32_t osd1;         // Campo específico do sistema operacional (OS dependent 1)
    uint32_t block[15];    // Ponteiros para blocos de dados (diretos, indiretos, duplamente indiretos, etc)
    uint32_t generation;   // Número de geração do arquivo (usado para NFS)
    uint32_t file_acl;     // Endereço do bloco de ACL do arquivo (controle de acesso estendido)
    uint32_t dir_acl;      // Tamanho alto do arquivo para arquivos grandes ou bloco de ACL para diretórios
    uint32_t faddr;        // Endereço do bloco de fragmento (não usado no Linux)
    uint8_t osd2[12];      // Campos adicionais específicos do sistema operacional (OS dependent 2)
} __attribute__((packed)) ext2_inode;

typedef struct {
    // Linked Directory Entry Structure
    uint32_t inode;      // Número do inode apontado por esta entrada de diretório
    uint16_t rec_len;    // Comprimento total desta entrada de diretório (em bytes)
    uint8_t name_len;    // Comprimento do nome do arquivo/diretório (em caracteres)
    uint8_t file_type;   // Tipo do arquivo (ex: arquivo regular, diretório, link simbólico, etc)
    char name[];         // Nome do arquivo/diretório (não terminado em '\0')
} __attribute__((packed)) ext2_dir_entry;

#endif
