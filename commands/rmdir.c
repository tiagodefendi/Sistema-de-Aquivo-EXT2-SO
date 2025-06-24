#include <stdio.h>
#include <string.h>
#include "rmdir.h"
#include "../ext2_utils.h"
#include "cd.h"

void cmd_rmdir(const char *dirname)
{
    if (!dirname || strlen(dirname) == 0)
    {
        printf("Uso: rmdir <diretorio>\n");
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

        if (strcmp(name, dirname) == 0)
        {
            ext2_inode target = read_inode(entry->inode);
            if ((target.mode & 0xF000) != 0x4000)
            {
                printf("Erro: %s não é um diretório.\n", dirname);
                return;
            }

            uint8_t target_block[BLOCK_SIZE];
            read_block(target.block[0], target_block);

            int inner_offset = 0;
            int entry_count = 0;
            while (inner_offset < BLOCK_SIZE)
            {
                ext2_dir_entry *subentry = (ext2_dir_entry *)(target_block + inner_offset);
                if (subentry->inode != 0)
                    entry_count++;
                inner_offset += subentry->rec_len;
            }

            if (entry_count > 2)
            {
                printf("Erro: diretório '%s' não está vazio.\n", dirname);
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

            uint32_t block_index = target.block[0] - 1;
            uint8_t bitmap[BLOCK_SIZE];
            read_block(group_desc.block_bitmap, bitmap);
            bitmap[block_index / 8] &= ~(1 << (block_index % 8));
            fseek(image_file, group_desc.block_bitmap * BLOCK_SIZE, SEEK_SET);
            fwrite(bitmap, 1, BLOCK_SIZE, image_file);
            superblock.free_blocks_count++;

            uint32_t inode_index = entry->inode - 1;
            read_block(group_desc.inode_bitmap, bitmap);
            bitmap[inode_index / 8] &= ~(1 << (inode_index % 8));
            fseek(image_file, group_desc.inode_bitmap * BLOCK_SIZE, SEEK_SET);
            fwrite(bitmap, 1, BLOCK_SIZE, image_file);
            superblock.free_inodes_count++;

            fseek(image_file, 1024, SEEK_SET);
            fwrite(&superblock, sizeof(ext2_super_block), 1, image_file);

            printf("Diretório '%s' removido com sucesso.\n", dirname);
            return;
        }

        prev_entry = (ext2_dir_entry *)(block + offset);
        offset += entry->rec_len;
    }

    printf("Diretório '%s' não encontrado.\n", dirname);
}
