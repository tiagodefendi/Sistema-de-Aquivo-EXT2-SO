#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cd.h"
#include "cp.h"
#include "../ext2_utils.h"

void cmd_cp(const char *src, const char *dst_path)
{
    if (!src || !dst_path || strlen(src) == 0 || strlen(dst_path) == 0)
    {
        printf("Uso: cp <arquivo_na_imagem> <caminho_destino_na_imagem>\n");
        return;
    }

    ext2_inode src_dir_inode = read_inode(get_current_inode());
    uint8_t block[BLOCK_SIZE];
    read_block(src_dir_inode.block[0], block);

    ext2_inode src_inode;
    int found = 0;

    int offset = 0;
    while (offset < BLOCK_SIZE)
    {
        ext2_dir_entry *entry = (ext2_dir_entry *)(block + offset);
        if (entry->inode == 0)
            break;

        char name[256];
        memcpy(name, entry->name, entry->name_len);
        name[entry->name_len] = '\0';

        if (strcmp(name, src) == 0)
        {
            src_inode = read_inode(entry->inode);
            if ((src_inode.mode & 0xF000) != 0x8000)
            {
                printf("Erro: %s não é um arquivo regular.\n", src);
                return;
            }
            found = 1;
            break;
        }
        offset += entry->rec_len;
    }

    if (!found)
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
        ext2_dir_entry *entry = (ext2_dir_entry *)(block + offset);
        if (entry->inode == 0)
            break;
        char name[256];
        memcpy(name, entry->name, entry->name_len);
        name[entry->name_len] = '\0';
        if (strcmp(name, newname) == 0)
        {
            printf("Aviso: '%s' já existe e será sobrescrito.\n", newname);
            break;
        }
        offset += entry->rec_len;
    }

    uint32_t new_inode_num = allocate_inode();
    if (new_inode_num == 0)
    {
        printf("Erro: sem inodes livres.\n");
        return;
    }

    ext2_inode new_inode = src_inode;
    new_inode.links_count = 1;

    for (int i = 0; i < 12; i++)
        new_inode.block[i] = src_inode.block[i];

    write_inode(new_inode_num, &new_inode);
    add_dir_entry(target_inode, new_inode_num, newname, 1);

    printf("Arquivo '%s' copiado com sucesso para '%s'.\n", src, dst_path);
}
