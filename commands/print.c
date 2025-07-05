#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "commands.h"

/**
 * @brief Imprime informações do superbloco do sistema de arquivos EXT2.
 *
 * Esta função recebe um ponteiro para o superbloco e imprime suas informações formatadas.
 *
 * @param sb Ponteiro para o superbloco do sistema de arquivos EXT2.
 */
void print_super(struct ext2_super_block *sb)
{
    printf("inodes count..................: %u\n", sb->s_inodes_count);
    printf("blocks count..................: %u\n", sb->s_blocks_count);
    printf("reserved blocks count.........: %u\n", sb->s_r_blocks_count);
    printf("free blocks count.............: %u\n", sb->s_free_blocks_count);
    printf("free inodes count.............: %u\n", sb->s_free_inodes_count);
    printf("first data block..............: %u\n", sb->s_first_data_block);
    printf("block size....................: %u\n", 1024 << sb->s_log_block_size);
    printf("fragment size.................: %u\n", 1024 << sb->s_log_frag_size);
    printf("blocks per group..............: %u\n", sb->s_blocks_per_group);
    printf("fragments per group...........: %u\n", sb->s_frags_per_group);
    printf("inodes per group..............: %u\n", sb->s_inodes_per_group);
    printf("mount time....................: %u\n", sb->s_mtime);
    printf("write time....................: %u\n", sb->s_wtime);
    printf("mount count...................: %u\n", sb->s_mnt_count);
    printf("max mount count...............: %u\n", sb->s_max_mnt_count);
    printf("magic signature...............: 0x%x\n", sb->s_magic);
    printf("file system state.............: %u\n", sb->s_state);
    printf("errors........................: %u\n", sb->s_errors);
    printf("minor revision level..........: %u\n", sb->s_minor_rev_level);
    time_t last = sb->s_lastcheck;
    char buf[32];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&last));
    printf("time of last check............: %s\n", buf);
    printf("max check interval............: %u\n", sb->s_checkinterval);
    printf("creator OS....................: %u\n", sb->s_creator_os);
    printf("revision level................: %u\n", sb->s_rev_level);
    printf("default uid reserved blocks...: %u\n", sb->s_def_resuid);
    printf("default gid reserved blocks...: %u\n", sb->s_def_resgid);
    printf("first non-reserved inode......: %u\n", sb->s_first_ino);
    printf("inode size....................: %u\n", sb->s_inode_size);
    printf("block group number............: %u\n", sb->s_block_group_nr);
    printf("compatible feature set........: %u\n", sb->s_feature_compat);
    printf("incompatible feature set......: %u\n", sb->s_feature_incompat);
    printf("read only comp feature set....: %u\n", sb->s_feature_ro_compat);
    printf("volume UUID...................: ");
    for (int i = 0; i < 16; ++i)
    {
        printf("%02x", sb->s_uuid[i]);
    }
    printf("\n");
    printf("volume name...................: %.*s\n", 16, sb->s_volume_name);
    printf("volume last mounted...........: %.*s\n", 64, sb->s_last_mounted);
    printf("algorithm usage bitmap........: %u\n", sb->s_algo_bitmap);
    printf("blocks to try to preallocate..: %u\n", sb->s_prealloc_blocks);
    printf("blocks preallocate dir........: %u\n", sb->s_prealloc_dir_blocks);
    printf("journal UUID..................: ");
    for (int i = 0; i < 16; ++i)
    {
        printf("%02x", sb->s_journal_uuid[i]);
    }
    printf("\n");
    printf("journal INum..................: %u\n", sb->s_journal_inum);
    printf("journal Dev...................: %u\n", sb->s_journal_dev);
    printf("last orphan...................: %u\n", sb->s_last_orphan);
    printf("hash seed.....................: ");
    for (int i = 0; i < 4; ++i)
    {
        printf("%08x", sb->s_hash_seed[i]);
    }
    printf("\n");
    printf("default hash version..........: %u\n", sb->s_def_hash_version);
    printf("default mount options.........: %u\n", sb->s_default_mount_options);
    printf("first meta....................: %u\n", sb->s_first_meta_bg);
}

/**
 * @brief Imprime informações dos grupos de blocos do sistema de arquivos EXT2.
 *
 * Esta função percorre todos os grupos de blocos e imprime suas informações formatadas.
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 */
void print_groups(ext2_fs_t *fs)
{
    struct ext2_group_desc gd;
    for (uint32_t g = 0; g < fs->groups_count; ++g)
    {
        if (fs_read_group_desc(fs, g, &gd) < 0)
            break;
        printf("Block Group Descriptor %u:\n", g);
        printf("    block bitmap.............: %u\n", gd.bg_block_bitmap);
        printf("    inode bitmap.............: %u\n", gd.bg_inode_bitmap);
        printf("    inode table..............: %u\n", gd.bg_inode_table);
        printf("    free blocks count........: %u\n", gd.bg_free_blocks_count);
        printf("    free inodes count........: %u\n", gd.bg_free_inodes_count);
        printf("    used dirs count..........: %u\n", gd.bg_used_dirs_count);
    }
}

/**
 * @brief Imprime informações de um inode específico do sistema de arquivos EXT2.
 *
 * Esta função lê o inode especificado pelo número e imprime suas informações formatadas.
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param ino Número do inode a ser impresso.
 */
void print_inode(ext2_fs_t *fs, uint32_t ino)
{
    struct ext2_inode in;
    if (fs_read_inode(fs, ino, &in) < 0)
    {
        perror("inode");
        return;
    }
    printf("file format and access rights..: 0x%x\n", in.i_mode);
    printf("user id........................: %u\n", in.i_uid);
    printf("lower 32-bit file size.........: %u\n", in.i_size);
    printf("access time....................: %u\n", in.i_atime);
    printf("creation time..................: %u\n", in.i_ctime);
    printf("modification time..............: %u\n", in.i_mtime);
    printf("deletion time..................: %u\n", in.i_dtime);
    printf("group id.......................: %u\n", in.i_gid);
    printf("link count inode...............: %u\n", in.i_links_count);
    printf("512-bytes blocks...............: %u\n", in.i_blocks);
    printf("ext2 flags.....................: %u\n", in.i_flags);
    printf("reserved (Linux)...............: %u\n", in.i_osd1);
    for (int i = 0; i < 15; ++i)
        printf("pointer[%2d].....................: %u\n", i, in.i_block[i]);
    printf("file version (nfs).............: %u\n", in.i_generation);
    printf("block number ext. attributes...: %u\n", in.i_file_acl);
    printf("higher 32-bit file size........: %u\n", in.i_dir_acl);
    printf("location file fragment.........: %u\n", in.i_faddr);
}

/**
 * @brief Comando para imprimir informações do sistema de arquivos EXT2.
 *
 * Este comando aceita argumentos para imprimir o superbloco, grupos de blocos ou um inode específico.
 *
 * @param argc Número de argumentos.
 * @param argv Array de argumentos.
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o diretório de trabalho atual (não utilizado neste comando).
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_print(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    (void)cwd;                // O diretório de trabalho atual não é utilizado neste comando
    if (argc < 2 || argc > 3) // Verifica se há argumentos suficientes
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "superblock") == 0) // Verifica se o comando é para imprimir o superbloco
    {
        if (argc != 2) // Verifica se o número de argumentos é correto
        {
            print_error(ERROR_INVALID_SYNTAX);
            return EXIT_FAILURE;
        }
        print_super(&fs->sb);
        return EXIT_SUCCESS;
    }
    if (strcmp(argv[1], "groups") == 0) // Verifica se o comando é para imprimir grupos de blocos
    {
        if (argc != 2) // Verifica se o número de argumentos é correto
        {
            print_error(ERROR_INVALID_SYNTAX);
            return EXIT_FAILURE;
        }
        print_groups(fs);
        return EXIT_SUCCESS;
    }
    if (strcmp(argv[1], "inode") == 0) // Verifica se o comando é para imprimir um inode específico
    {
        if (argc != 3) // Verifica se o número de argumentos é correto
        {
            print_error(ERROR_INVALID_SYNTAX);
            return EXIT_FAILURE;
        }
        uint32_t ino = strtoul(argv[2], NULL, 0); // Converte o argumento para um número inteiro sem sinal
        print_inode(fs, ino);
        return EXIT_SUCCESS;
    }
    print_error(ERROR_INVALID_SYNTAX); // Se nenhum dos comandos for reconhecido, imprime erro de sintaxe
    return EXIT_FAILURE;
}
