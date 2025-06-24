#include <stdio.h>
#include <string.h>
#include <time.h>
#include "attr.h"
#include "../ext2_utils.h"
#include "cd.h"

void cmd_attr(const char *name)
{
    if (!name || strlen(name) == 0)
    {
        printf("Uso: attr <arquivo|diretorio>\n");
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

        char entry_name[256];
        memcpy(entry_name, entry->name, entry->name_len);
        entry_name[entry->name_len] = '\0';

        if (strcmp(entry_name, name) == 0)
        {
            ext2_inode target = read_inode(entry->inode);

            char type = '-';
            if ((target.mode & 0xF000) == 0x4000)
                type = 'd';
            else if ((target.mode & 0xF000) == 0x8000)
                type = '-';

            printf("permissões  uid  gid  tamanho  modificado em\n");
            printf("%crw-r--r--  %u  %u  %.1f KiB  ",
                    type,
                    target.uid,
                    target.gid,
                    target.size / 1024.0);

            time_t mtime = target.mtime;
            struct tm *tm_info = localtime(&mtime);
            char date_buf[64];
            strftime(date_buf, sizeof(date_buf), "%d/%m/%Y %H:%M", tm_info);
            printf("%s\n", date_buf);
            return;
        }

        offset += entry->rec_len;
    }

    printf("Arquivo ou diretório '%s' não encontrado.\n", name);
}
