/*
 * Exibe um resumo do volume ext2 carregado.
 */

#include <stdio.h>
#include <stdint.h>
#include "ext2.h"
#include "utils.h"

/*
 * cmd_info(img)
 *   Imprime informações gerais sobre a imagem/ext2.
 *   Retorna 0 em sucesso, -1 em erro.
 */
int cmd_info(FILE *img)
{
    struct ext2_super_block sb;

    /* Lê superbloco; aborta se assinatura inválida */
    if (read_superblock(img, &sb) != 0)
    {
        fprintf(stderr, "info: unable to read superblock or invalid image.\n");
        return -1;
    }

    /* ---------- Cálculos derivados (somente inteiros) ---------- */
    uint64_t image_size = (uint64_t)sb.s_blocks_count * EXT2_BLOCK_SIZE;
    uint32_t free_blocks = sb.s_free_blocks_count;
    uint32_t free_space_kib = free_blocks; /* bloco fixo 1 KiB -> 1:1 */
    uint32_t groups_count = (sb.s_blocks_count + sb.s_blocks_per_group - 1) /
                            sb.s_blocks_per_group;
    uint32_t groups_size_blk = sb.s_blocks_per_group;
    uint32_t groups_inodes = sb.s_inodes_per_group;
    uint32_t inodetable_size = (sb.s_inodes_per_group * sb.s_inode_size) /
                               EXT2_BLOCK_SIZE;

    /* ---------- Saída formatada ---------- */
    printf("Volume name.....: %.16s\n", sb.s_volume_name);
    printf("Image size......: %llu bytes\n", (unsigned long long)image_size);
    printf("Free space......: %u KiB\n", free_space_kib);
    printf("Free inodes.....: %u\n", sb.s_free_inodes_count);
    printf("Free blocks.....: %u\n", free_blocks);
    printf("Block size......: %u bytes\n", EXT2_BLOCK_SIZE);
    printf("Inode size......: %u bytes\n", sb.s_inode_size);
    printf("Groups count....: %u\n", groups_count);
    printf("Groups size.....: %u blocks\n", groups_size_blk);
    printf("Groups inodes...: %u inodes\n", groups_inodes);
    printf("Inodetable size.: %u blocks\n", inodetable_size);

    return 0;
}
