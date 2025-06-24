#include <stdio.h>
#include "../ext2_utils.h"
#include "info.h"

void cmd_info()
{
    printf("Volume name.....: %.16s\n", superblock.volume_name);
    printf("Image size......: %u bytes\n", BLOCK_SIZE * superblock.blocks_count);
    printf("Free space......: %u KiB\n", (superblock.free_blocks_count * BLOCK_SIZE) / 1024);
    printf("Free inodes.....: %u\n", superblock.free_inodes_count);
    printf("Free blocks.....: %u\n", superblock.free_blocks_count);
    printf("Block size......: %u bytes\n", BLOCK_SIZE);
    printf("Inode size......: %u bytes\n", superblock.inode_size);
    printf("Groups count....: %u\n", (superblock.blocks_count / superblock.blocks_per_group));
    printf("Groups size.....: %u blocks\n", superblock.blocks_per_group);
    printf("Groups inodes...: %u inodes\n", superblock.inodes_per_group);
    printf("Inodetable size.: %u blocks\n", superblock.inodes_per_group * superblock.inode_size / BLOCK_SIZE);
}
