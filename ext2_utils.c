#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ext2_structs.h"

#define BLOCK_SIZE 1024

FILE *image_file = NULL;
char image_path[256];
ext2_super_block superblock;
ext2_group_desc group_desc;

int load_image(const char *path)
{
    strncpy(image_path, path, sizeof(image_path));
    image_file = fopen(path, "rb+");
    if (!image_file)
    {
        perror("Erro ao abrir a imagem");
        return -1;
    }

    fseek(image_file, 1024, SEEK_SET);
    fread(&superblock, sizeof(ext2_super_block), 1, image_file);

    if (superblock.magic != 0xEF53)
    {
        fprintf(stderr, "Imagem não possui assinatura EXT2 válida\n");
        return -1;
    }

    fseek(image_file, 2048, SEEK_SET);
    fread(&group_desc, sizeof(ext2_group_desc), 1, image_file);

    return 0;
}

void read_block(uint32_t block_num, void *buffer)
{
    fseek(image_file, block_num * BLOCK_SIZE, SEEK_SET);
    fread(buffer, BLOCK_SIZE, 1, image_file);
}

ext2_inode read_inode(uint32_t inode_num)
{
    ext2_inode inode;
    uint32_t index = inode_num - 1;
    uint32_t inodes_per_block = BLOCK_SIZE / superblock.inode_size;
    uint32_t index_in_group = index % superblock.inodes_per_group;
    uint32_t block = group_desc.inode_table + (index_in_group / inodes_per_block);
    uint32_t offset = (index_in_group % inodes_per_block) * superblock.inode_size;

    fseek(image_file, block * BLOCK_SIZE + offset, SEEK_SET);
    fread(&inode, sizeof(ext2_inode), 1, image_file);
    return inode;
}

void print_superblock_info()
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

uint32_t allocate_inode()
{
    uint8_t bitmap[BLOCK_SIZE];
    read_block(group_desc.inode_bitmap, bitmap);
    for (uint32_t i = 0; i < superblock.inodes_per_group; i++)
    {
        if (!(bitmap[i / 8] & (1 << (i % 8))))
        {
            bitmap[i / 8] |= (1 << (i % 8));
            fseek(image_file, group_desc.inode_bitmap * BLOCK_SIZE, SEEK_SET);
            fwrite(bitmap, 1, BLOCK_SIZE, image_file);
            superblock.free_inodes_count--;
            fseek(image_file, 1024, SEEK_SET);
            fwrite(&superblock, sizeof(ext2_super_block), 1, image_file);
            return i + 1;
        }
    }
    return 0;
}

uint32_t allocate_block()
{
    uint8_t bitmap[BLOCK_SIZE];
    read_block(group_desc.block_bitmap, bitmap);
    for (uint32_t i = 0; i < superblock.blocks_per_group; i++)
    {
        if (!(bitmap[i / 8] & (1 << (i % 8))))
        {
            bitmap[i / 8] |= (1 << (i % 8));
            fseek(image_file, group_desc.block_bitmap * BLOCK_SIZE, SEEK_SET);
            fwrite(bitmap, 1, BLOCK_SIZE, image_file);
            superblock.free_blocks_count--;
            fseek(image_file, 1024, SEEK_SET);
            fwrite(&superblock, sizeof(ext2_super_block), 1, image_file);
            return i + 1;
        }
    }
    return 0;
}

void write_inode(uint32_t inode_num, ext2_inode *inode)
{
    uint32_t index = inode_num - 1;
    uint32_t inodes_per_block = BLOCK_SIZE / superblock.inode_size;
    uint32_t index_in_group = index % superblock.inodes_per_group;
    uint32_t block = group_desc.inode_table + (index_in_group / inodes_per_block);
    uint32_t offset = (index_in_group % inodes_per_block) * superblock.inode_size;
    fseek(image_file, block * BLOCK_SIZE + offset, SEEK_SET);
    fwrite(inode, sizeof(ext2_inode), 1, image_file);
}

void add_dir_entry(uint32_t dir_inode_num, uint32_t new_inode_num, const char *name, uint8_t file_type)
{
    ext2_inode dir_inode = read_inode(dir_inode_num);
    uint8_t block[BLOCK_SIZE];
    read_block(dir_inode.block[0], block);

    int offset = 0;
    ext2_dir_entry *entry;
    while (offset < BLOCK_SIZE)
    {
        entry = (ext2_dir_entry *)(block + offset);
        if (offset + entry->rec_len >= BLOCK_SIZE)
            break;
        offset += entry->rec_len;
    }

    uint16_t name_len = strlen(name);
    uint16_t prev_len = entry->rec_len;
    uint16_t actual_len = 8 + ((entry->name_len + 3) & ~3);
    entry->rec_len = actual_len;

    ext2_dir_entry *new_entry = (ext2_dir_entry *)(block + offset + actual_len);
    new_entry->inode = new_inode_num;
    new_entry->rec_len = prev_len - actual_len;
    new_entry->name_len = name_len;
    new_entry->file_type = file_type;
    memcpy(new_entry->name, name, name_len);

    fseek(image_file, dir_inode.block[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(block, 1, BLOCK_SIZE, image_file);
}

int resolve_cp_target(const char *path, uint32_t *target_dir_inode, char *final_name, const char *default_name)
{
    if (path[0] != '/')
        return 0;
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy));
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *tokens[32];
    int count = 0;
    char *token = strtok(path_copy, "/");
    while (token && count < 31)
    {
        tokens[count++] = token;
        token = strtok(NULL, "/");
    }
    if (count == 0)
        return 0;

    uint32_t inode_num = 2;
    for (int i = 0; i < count; i++)
    {
        ext2_inode inode = read_inode(inode_num);
        uint8_t block[BLOCK_SIZE];
        read_block(inode.block[0], block);

        int offset = 0, found = 0;
        while (offset < BLOCK_SIZE)
        {
            ext2_dir_entry *entry = (ext2_dir_entry *)(block + offset);
            if (entry->inode == 0)
                break;

            char name[256];
            memcpy(name, entry->name, entry->name_len);
            name[entry->name_len] = '\0';

            if (strcmp(name, tokens[i]) == 0)
            {
                if (i == count - 1)
                {
                    ext2_inode dest_inode = read_inode(entry->inode);
                    if ((dest_inode.mode & 0xF000) == 0x4000)
                    {
                        *target_dir_inode = entry->inode;
                        strcpy(final_name, default_name);
                    }
                    else
                    {
                        *target_dir_inode = inode_num;
                        strcpy(final_name, tokens[i]);
                    }
                    return 1;
                }
                else
                {
                    inode_num = entry->inode;
                    found = 1;
                    break;
                }
            }
            offset += entry->rec_len;
        }

        if (!found)
        {
            if (i == count - 1)
            {
                *target_dir_inode = inode_num;
                strcpy(final_name, tokens[i]);
                return 1;
            }
            return 0;
        }
    }

    return 0;
}

void close_image()
{
    if (image_file)
        fclose(image_file);
}
