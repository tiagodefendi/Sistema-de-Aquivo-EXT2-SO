#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "commands.h"
#include "utils.h"

/**
 * @brief Libera blocos indiretos (simples, duplo, triplo) sem logs
 */
static int free_indirect_chain(ext2_fs_t *fs, uint32_t blk, int depth) {
    if (!blk) return EXIT_SUCCESS;
    uint32_t ptrs[EXT2_BLOCK_SIZE/sizeof(uint32_t)];
    if (fs_read_block(fs, blk, ptrs) < 0) {
        return EXIT_FAILURE;
    }
    for (int i = 0; i < (int)(EXT2_BLOCK_SIZE/sizeof(uint32_t)); i++) {
        uint32_t b = ptrs[i];
        if (!b) continue;
        if (depth > 1) {
            if (free_indirect_chain(fs, b, depth-1) == EXIT_FAILURE)
                return EXIT_FAILURE;
        } else {
            if (fs_free_block(fs, b) < 0) {
                return EXIT_FAILURE;
            }
        }
    }
    if (fs_free_block(fs, blk) < 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @brief Libera blocos diretos e indiretos de um inode, sem logs
 */
static int free_inode_blocks_logged(ext2_fs_t *fs, struct ext2_inode *inode) {
    // diretos
    for (int i = 0; i < 12; i++) {
        uint32_t b = inode->i_block[i];
        if (!b) continue;
        if (fs_free_block(fs, b) < 0) {
            return EXIT_FAILURE;
        }
    }
    // indireto simples, duplo, triplo
    if (free_indirect_chain(fs, inode->i_block[12], 1) == EXIT_FAILURE) return EXIT_FAILURE;
    if (free_indirect_chain(fs, inode->i_block[13], 2) == EXIT_FAILURE) return EXIT_FAILURE;
    if (free_indirect_chain(fs, inode->i_block[14], 3) == EXIT_FAILURE) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

/**
 * @brief Verifica se diretório está vazio (exceto . e ..)
 */
static int is_directory_empty(ext2_fs_t *fs, struct ext2_inode *dir_inode) {
    uint8_t buf[EXT2_BLOCK_SIZE];
    for (int i = 0; i < 12; i++) {
        uint32_t b = dir_inode->i_block[i];
        if (!b) continue;
        if (fs_read_block(fs, b, buf) < 0) return -1;
        uint32_t off = 0;
        while (off < EXT2_BLOCK_SIZE) {
            struct ext2_dir_entry *e = (void*)(buf+off);
            if (e->rec_len == 0) break;
            if (e->inode && !(e->name_len==1&&e->name[0]=='.') && !(e->name_len==2&&e->name[0]=='.'&&e->name[1]=='.')) {
                return 0;
            }
            off += e->rec_len;
        }
    }
    return 1;
}

/**
 * @brief Remove entrada de diretório pelo inode, sem logs
 */
static int dir_remove_entry_rm(ext2_fs_t *fs, struct ext2_inode *parent_inode, uint32_t target_ino) {
    uint8_t buf[EXT2_BLOCK_SIZE];
    for (int i = 0; i < 12; i++) {
        uint32_t b = parent_inode->i_block[i];
        if (!b) continue;
        if (fs_read_block(fs, b, buf) < 0) return EXIT_FAILURE;
        uint32_t off = 0;
        struct ext2_dir_entry *prev = NULL;
        while (off < EXT2_BLOCK_SIZE) {
            struct ext2_dir_entry *e = (void*)(buf+off);
            if (e->rec_len == 0) break;
            if (e->inode == target_ino) {
                if (prev) prev->rec_len += e->rec_len;
                else { e->inode=0; e->rec_len=EXT2_BLOCK_SIZE; }
                if (fs_write_block(fs, b, buf) < 0) return EXIT_FAILURE;
                return EXIT_SUCCESS;
            }
            prev = e;
            off += e->rec_len;
        }
    }
    return EXIT_FAILURE;
}

int cmd_rmdir(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd) {
    if (argc != 2) {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }
    char *full_path = fs_join_path(fs, *cwd, argv[1]);
    if (!full_path) {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    uint32_t dir_ino;
    if (fs_path_resolve(fs, full_path, &dir_ino) < 0) {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }
    struct ext2_inode dir_inode;
    if (fs_read_inode(fs, dir_ino, &dir_inode) < 0 || !ext2_is_dir(&dir_inode)) {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }
    int empty = is_directory_empty(fs, &dir_inode);
    if (empty < 0) {
        free(full_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (!empty) {
        free(full_path);
        print_error(ERROR_DIRECTORY_NOT_EMPTY);
        return EXIT_FAILURE;
    }
    char *parent_path = strdup(full_path);
    char *slash = strrchr(parent_path, '/');
    if (slash && slash != parent_path) *slash = '\0';
    else strcpy(parent_path, "/");
    uint32_t parent_ino;
    if (fs_path_resolve(fs, parent_path, &parent_ino) < 0) {
        free(full_path); free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    struct ext2_inode parent_inode;
    fs_read_inode(fs, parent_ino, &parent_inode);
    if (dir_remove_entry_rm(fs, &parent_inode, dir_ino) == EXIT_FAILURE) {
        free(full_path); free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    parent_inode.i_links_count--;
    fs_write_inode(fs, parent_ino, &parent_inode);
    if (free_inode_blocks_logged(fs, &dir_inode) == EXIT_FAILURE) {
        free(full_path); free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (fs_free_inode(fs, dir_ino) < 0) {
        free(full_path); free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    fs_sync_super(fs);
    free(full_path); free(parent_path);
    return EXIT_SUCCESS;
}
