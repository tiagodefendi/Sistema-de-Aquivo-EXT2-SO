#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cd.h"
#include "pwd.h"
#include "../ext2_utils.h"

static uint32_t current_inode_num = 2;

uint32_t get_current_inode()
{
    return current_inode_num;
}

int cmd_cd(const char *dirname)
{
    if (!dirname || strlen(dirname) == 0)
        return -1;

    ext2_inode dir_inode = read_inode(current_inode_num);
    uint8_t block[BLOCK_SIZE];
    read_block(dir_inode.block[0], block);

    int offset = 0;
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
            if ((target.mode & 0xF000) == 0x4000)
            {
                current_inode_num = entry->inode;

                if (strcmp(dirname, "..") == 0)
                {
                    char *last = strrchr(get_current_path(), '/');
                    if (last && last != get_current_path())
                    {
                        *last = '\0';
                    }
                    else
                    {
                        strcpy(get_current_path_mutable(), "/");
                    }
                }
                else if (strcmp(dirname, ".") != 0)
                {
                    if (strcmp(get_current_path_mutable(), "/") != 0)
                        strcat(get_current_path_mutable(), "/");
                    strcat(get_current_path_mutable(), dirname);
                }
                return 0;
            }
            else
            {
                printf("<%s> não é um diretório.\n", dirname);
                return -1;
            }
        }
        offset += entry->rec_len;
    }

    printf("<%s>: diretório não encontrado.\n", dirname);
    return -1;
}
