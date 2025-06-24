#include <stdio.h>
#include <string.h>
#include "rm.h"
#include "../ext2_utils.h"
#include "cd.h"

void cmd_rm(const char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Uso: rm <arquivo>\n");
        return;
    }

    ext2_inode dir_inode = read_inode(get_current_inode());
    uint8_t block[BLOCK_SIZE];
    read_block(dir_inode.block[0], block);

    int offset = 0;
    ext2_dir_entry *prev_entry = NULL;

    while (offset < BLOCK_SIZE)
    {
        ext2_dir_entry *entry = (ext2_dir_entry *)(block + offset);
        if (entry->inode == 0)
            break;

        char name[256];
        memcpy(name, entry->name, entry->name_len);
        name[entry->name_len] = '\0';

        if (strcmp(name, filename) == 0)
        {
            ext2_inode target = read_inode(entry->inode);
            if ((target.mode & 0xF000) != 0x8000)
            {
                printf("Erro: %s não é um arquivo regular.\n", filename);
                return;
            }

            if (prev_entry)
            {
                prev_entry->rec_len += entry->rec_len;
            }
            else
            {
                entry->inode = 0;
            }

            fseek(image_file, dir_inode.block[0] * BLOCK_SIZE, SEEK_SET);
            fwrite(block, 1, BLOCK_SIZE, image_file);

            uint32_t inode_index = entry->inode - 1;
            uint8_t bitmap[BLOCK_SIZE];
            read_block(group_desc.inode_bitmap, bitmap);
            bitmap[inode_index / 8] &= ~(1 << (inode_index % 8));
            fseek(image_file, group_desc.inode_bitmap * BLOCK_SIZE, SEEK_SET);
            fwrite(bitmap, 1, BLOCK_SIZE, image_file);

            superblock.free_inodes_count++;
            fseek(image_file, 1024, SEEK_SET);
            fwrite(&superblock, sizeof(ext2_super_block), 1, image_file);

            printf("%s removido com sucesso.\n", filename);
            return;
        }

        prev_entry = (ext2_dir_entry *)(block + offset);
        offset += entry->rec_len;
    }

    printf("Arquivo '%s' não encontrado.\n", filename);
}
