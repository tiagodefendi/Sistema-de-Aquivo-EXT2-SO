#include <stdio.h>
#include <string.h>
#include <time.h>
#include "touch.h"
#include "../ext2_utils.h"
#include "cd.h"

void cmd_touch(const char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Uso: touch <nome_do_arquivo>\n");
        return;
    }

    ext2_inode dir_inode = read_inode(get_current_inode());
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

        if (strcmp(name, filename) == 0)
        {
            printf("%s já existe.\n", filename);
            return;
        }

        offset += entry->rec_len;
    }

    uint32_t new_inode_num = allocate_inode();
    if (new_inode_num == 0)
    {
        printf("Erro: sem inodes livres.\n");
        return;
    }

    ext2_inode new_inode = {0};
    time_t now = time(NULL);
    new_inode.mode = 0x81A4;
    new_inode.uid = 0;
    new_inode.gid = 0;
    new_inode.size = 0;
    new_inode.atime = new_inode.ctime = new_inode.mtime = now;
    new_inode.links_count = 1;
    new_inode.blocks = 0;

    write_inode(new_inode_num, &new_inode);
    add_dir_entry(get_current_inode(), new_inode_num, filename, 1);

    printf("%s criado com sucesso.\n", filename);
}
