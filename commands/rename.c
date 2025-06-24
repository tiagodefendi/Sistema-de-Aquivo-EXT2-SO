#include <stdio.h>
#include <string.h>
#include "rename.h"
#include "../ext2_utils.h"
#include "cd.h"

void cmd_rename(const char *oldname, const char *newname)
{
    if (!oldname || !newname || strlen(oldname) == 0 || strlen(newname) == 0)
    {
        printf("Uso: rename <nome_antigo> <novo_nome>\n");
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

        if (strcmp(name, oldname) == 0)
        {
            if (strlen(newname) > 255)
            {
                printf("Erro: nome muito longo.\n");
                return;
            }

            entry->name_len = strlen(newname);
            memcpy(entry->name, newname, entry->name_len);

            fseek(image_file, dir_inode.block[0] * BLOCK_SIZE, SEEK_SET);
            fwrite(block, 1, BLOCK_SIZE, image_file);

            printf("Arquivo/diretório '%s' renomeado para '%s'.\n", oldname, newname);
            return;
        }

        offset += entry->rec_len;
    }

    printf("Erro: '%s' não encontrado no diretório atual.\n", oldname);
}
