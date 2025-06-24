/* ext2_utils.h - Header das funções utilitárias EXT2 */
#ifndef EXT2_UTILS_H
#define EXT2_UTILS_H

#include <stdio.h>
#include <stdint.h>
#include "ext2_structs.h"

#define BLOCK_SIZE 1024

extern FILE *image_file;
extern char image_path[256];
extern ext2_super_block superblock;
extern ext2_group_desc group_desc;

int load_image(const char *path);
void close_image();
void read_block(uint32_t block_num, void *buffer);
ext2_inode read_inode(uint32_t inode_num);
void print_superblock_info();

uint32_t allocate_inode();
uint32_t allocate_block();
void write_inode(uint32_t inode_num, ext2_inode *inode);

void add_dir_entry(uint32_t dir_inode_num, uint32_t new_inode_num, const char *name, uint8_t file_type);
int resolve_cp_target(const char *path, uint32_t *target_dir_inode, char *final_name, const char *default_name);

#endif
