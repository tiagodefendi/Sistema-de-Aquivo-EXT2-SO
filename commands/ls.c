#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../ext2_utils.h"
#include "ls.h"
#include "cd.h"

void cmd_ls()
{
    ext2_inode dir_inode = read_inode(get_current_inode());
    if ((dir_inode.mode & 0xF000) != 0x4000)
    {
        printf("Diretório atual não é válido.\n");
        return;
    }

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

        printf("%s\n", name);
        printf("inode: %u\n", entry->inode);
        printf("record length: %u\n", entry->rec_len);
        printf("name length: %u\n", entry->name_len);
        printf("file type: %u\n\n", entry->file_type);

        offset += entry->rec_len;
    }
}
