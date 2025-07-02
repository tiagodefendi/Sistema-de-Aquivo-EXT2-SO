#include "utils.h"
#include <string.h>
#include <errno.h>

/* ---------------- Superbloco ---------------- */
int read_superblock(FILE *img, struct ext2_super_block *sb)
{
    if (fseek(img, 1024, SEEK_SET) != 0)
        return -1;
    if (fread(sb, sizeof(struct ext2_super_block), 1, img) != 1)
        return -1;
    return (sb->s_magic == EXT2_SUPER_MAGIC) ? 0 : -1;
}

/* ---------------- Descritor de grupo ---------------- */
int read_group_desc(FILE *img, const struct ext2_super_block *sb,
                    struct ext2_group_desc *gd)
{
    /* Para bloco de 1 KiB, a tabela começa logo após o superbloco */
    long off = 1024 + EXT2_BLOCK_SIZE;
    if (fseek(img, off, SEEK_SET) != 0)
        return -1;
    return (fread(gd, sizeof(struct ext2_group_desc), 1, img) == 1) ? 0 : -1;
}

/* ---------------- Inode ---------------- */
int read_inode(FILE *img, const struct ext2_super_block *sb,
               const struct ext2_group_desc *gd, uint32_t inode_no,
               struct ext2_inode *inode)
{
    if (inode_no == 0 || inode_no > sb->s_inodes_count)
        return -1;

    uint32_t idx = inode_no - 1; /* zero‑based index */
    uint32_t block = gd->bg_inode_table + (idx / EXT2_INODES_PER_BLOCK);
    uint32_t offset = EXT2_BLOCK_OFFSET(block) +
                      (idx % EXT2_INODES_PER_BLOCK) * EXT2_INODE_SIZE;

    if (fseek(img, offset, SEEK_SET) != 0)
        return -1;
    return (fread(inode, sizeof(struct ext2_inode), 1, img) == 1) ? 0 : -1;
}

/* ---------------- Decodificação de modo ---------------- */
char mode_to_filetype(uint16_t mode)
{
    printf("mode:  0x%04X\n", mode);
    /* Máscara 0xF000 isola os bits de tipo */
    switch (mode & 0xF000)
    {
    case EXT2_S_IFDIR:
        return 'd'; /* diretório */
    case EXT2_S_IFREG:
        return 'f'; /* arquivo regular */
    case EXT2_S_IFLNK:
        return 'l'; /* link simbólico */
    case EXT2_S_IFCHR:
        return 'c'; /* char device */
    case EXT2_S_IFBLK:
        return 'b'; /* block device */
    case EXT2_S_IFSOCK:
        return 's'; /* socket */
    case EXT2_S_IFIFO:
        return 'p'; /* fifo */
    default:
        return '?';
    }
}

static const uint16_t perm_bits[9] = {
    EXT2_S_IRUSR, EXT2_S_IWUSR, EXT2_S_IXUSR,
    EXT2_S_IRGRP, EXT2_S_IWGRP, EXT2_S_IXGRP,
    EXT2_S_IROTH, EXT2_S_IWOTH, EXT2_S_IXOTH};

void mode_to_permstr(uint16_t mode, char perm[11])
{
    perm[0] = mode_to_filetype(mode);
    for (int i = 0; i < 9; ++i)
        perm[i + 1] = (mode & perm_bits[i]) ? "rwx"[i % 3] : '-';
    perm[10] = '\0';
}

/* ---------------- Outras utilidades ---------------- */
uint32_t inode_table_block(const struct ext2_group_desc *gd)
{
    return gd->bg_inode_table;
}

void print_superblock(const struct ext2_super_block *sb)
{
    printf("Blocos: %u  Inodes: %u  Blocos/Grupo: %u  Inodes/Grupo: %u  Rev: %u\n",
           sb->s_blocks_count, sb->s_inodes_count,
           sb->s_blocks_per_group, sb->s_inodes_per_group,
           sb->s_rev_level);
}

/* --- I/O de bloco --- */
int read_block(FILE *img, uint32_t block, void *buf)
{
    if (!block)
        return -1;
    if (fseek(img, EXT2_BLOCK_OFFSET(block), SEEK_SET) != 0)
        return -1;
    return fread(buf, EXT2_BLOCK_SIZE, 1, img) == 1 ? 0 : -1;
}