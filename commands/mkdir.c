#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mkdir.h"
#include "cd.h"
#include "../ext2_utils.h"

void cmd_mkdir(const char *dirname) {
    if (!dirname || strlen(dirname) == 0) {
        printf("Uso: mkdir <nome_do_diretorio>\n");
        return;
    }

    ext2_inode dir_inode = read_inode(get_current_inode());
    uint8_t block[BLOCK_SIZE];
    read_block(dir_inode.block[0], block);

    int offset = 0;
    while (offset < BLOCK_SIZE) {
        ext2_dir_entry *entry = (ext2_dir_entry *)(block + offset);
        if (entry->inode == 0) break;

        char name[256];
        memcpy(name, entry->name, entry->name_len);
        name[entry->name_len] = '\0';

        if (strcmp(name, dirname) == 0) {
            printf("%s já existe.\n", dirname);
            return;
        }

        offset += entry->rec_len;
    }

    uint32_t new_inode_num = allocate_inode();
    if (new_inode_num == 0) {
        printf("Erro: sem inodes livres.\n");
        return;
    }

    uint32_t new_block_num = allocate_block();
    if (new_block_num == 0) {
        printf("Erro: sem blocos livres.\n");
        return;
    }

    ext2_inode new_dir = {0};
    time_t now = time(NULL);
    new_dir.mode = 0x41ED;
    new_dir.uid = 0;
    new_dir.gid = 0;
    new_dir.size = BLOCK_SIZE;
    new_dir.atime = new_dir.ctime = new_dir.mtime = now;
    new_dir.links_count = 2;
    new_dir.blocks = 2;
    new_dir.block[0] = new_block_num;

    write_inode(new_inode_num, &new_dir);
    add_dir_entry(get_current_inode(), new_inode_num, dirname, 2);

    uint8_t dir_block[BLOCK_SIZE] = {0};
    ext2_dir_entry *self = (ext2_dir_entry *)dir_block;
    self->inode = new_inode_num;
    self->rec_len = 12;
    self->name_len = 1;
    self->file_type = 2;
    self->name[0] = '.';

    ext2_dir_entry *parent = (ext2_dir_entry *)(dir_block + 12);
    parent->inode = get_current_inode();
    parent->rec_len = BLOCK_SIZE - 12;
    parent->name_len = 2;
    parent->file_type = 2;
    parent->name[0] = '.';
    parent->name[1] = '.';

    fseek(image_file, new_block_num * BLOCK_SIZE, SEEK_SET);
    fwrite(dir_block, 1, BLOCK_SIZE, image_file);

    printf("Diretório %s criado com sucesso.\n", dirname);
}