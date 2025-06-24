#include <stdio.h>
#include <string.h>
#include "cd.h"
#include "mv.h"
#include "../ext2_utils.h"

void cmd_mv(const char *src, const char *dst_path)
{
    if (!src || !dst_path || strlen(src) == 0 || strlen(dst_path) == 0)
    {
        printf("Uso: mv <arquivo> <destino>\n");
        return;
    }

    ext2_inode dir_inode = read_inode(get_current_inode());
    uint8_t block[BLOCK_SIZE];
    read_block(dir_inode.block[0], block);

    int offset = 0;
    ext2_dir_entry *entry = NULL;
    ext2_dir_entry *prev = NULL;
    uint32_t src_inode_num = 0;

    while (offset < BLOCK_SIZE)
    {
        entry = (ext2_dir_entry *)(block + offset);
        if (entry->inode == 0)
            break;

        char name[256];
        memcpy(name, entry->name, entry->name_len);
        name[entry->name_len] = '\0';

        if (strcmp(name, src) == 0)
        {
            src_inode_num = entry->inode;
            break;
        }
        prev = entry;
        offset += entry->rec_len;
    }

    if (src_inode_num == 0)
    {
        printf("Arquivo '%s' não encontrado.\n", src);
        return;
    }

    uint32_t target_inode;
    char newname[256];
    if (!resolve_cp_target(dst_path, &target_inode, newname, src))
    {
        printf("Destino inválido: %s\n", dst_path);
        return;
    }

    ext2_inode target_dir_inode = read_inode(target_inode);
    read_block(target_dir_inode.block[0], block);
    offset = 0;
    while (offset < BLOCK_SIZE)
    {
        ext2_dir_entry *e = (ext2_dir_entry *)(block + offset);
        if (e->inode == 0)
            break;
        char existing[256];
        memcpy(existing, e->name, e->name_len);
        existing[e->name_len] = '\0';
        if (strcmp(existing, newname) == 0 && e->inode == src_inode_num)
        {
            printf("Movimento desnecessário: origem e destino são o mesmo arquivo.\n");
            return;
        }
        offset += e->rec_len;
    }

    add_dir_entry(target_inode, src_inode_num, newname, 1);

    read_block(dir_inode.block[0], block);
    offset = 0;
    prev = NULL;
    while (offset < BLOCK_SIZE)
    {
        ext2_dir_entry *e = (ext2_dir_entry *)(block + offset);
        if (e->inode == 0)
            break;
        char name[256];
        memcpy(name, e->name, e->name_len);
        name[e->name_len] = '\0';
        if (strcmp(name, src) == 0)
        {
            if (prev)
            {
                prev->rec_len += e->rec_len;
            }
            else
            {
                e->inode = 0;
            }
            break;
        }
        prev = e;
        offset += e->rec_len;
    }
    fseek(image_file, dir_inode.block[0] * BLOCK_SIZE, SEEK_SET);
    fwrite(block, 1, BLOCK_SIZE, image_file);

    printf("Arquivo '%s' movido para '%s'.\n", src, dst_path);
}
