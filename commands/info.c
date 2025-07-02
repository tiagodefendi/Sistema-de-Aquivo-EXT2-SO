#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include "commands.h"

/**
 * @brief   Exibe informações sobre o volume EXT2.
 *
 * Esta função imprime detalhes do volume, como nome, tamanho da imagem,
 * espaço livre, contagem de blocos e inodes, e outras informações do superbloco.
 *
 * @param argc Número de argumentos (deve ser 1).
 * @param argv Vetor de argumentos (não utilizado).
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o diretório corrente (não utilizado).
 *
 * @return Retorna EXIT_SUCCESS em caso de sucesso, ou EXIT_FAILURE em caso de erro.
 */
int cmd_info(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    (void)argv;
    (void)cwd; // Diretório corrente não utilizado

    if (argc != 1) {
        fprintf(stderr, "Uso: info\n");
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    // Volume name pode não ser NUL-terminated
    char volume_name[17] = {0};
    memcpy(volume_name, fs->sb.s_volume_name, 16);

    // Obtém o tamanho total da imagem
    off_t image_size = lseek(fs->fd, 0, SEEK_END);
    if (image_size < 0) {
        perror("Erro ao obter tamanho da imagem");
        return EXIT_FAILURE;
    }

    uint32_t free_blocks = fs->sb.s_free_blocks_count;
    uint32_t free_inodes = fs->sb.s_free_inodes_count;
    uint32_t block_size = EXT2_BLOCK_SIZE; // Fixo em 1 KiB neste projeto
    uint32_t inode_size = fs->sb.s_inode_size; // 128 B
    uint32_t group_count = fs->groups_count;
    uint32_t blocks_per_group = fs->sb.s_blocks_per_group;
    uint32_t inodes_per_group = fs->sb.s_inodes_per_group;
    uint32_t inodetable_blocks = (inodes_per_group * inode_size + block_size - 1) / block_size;

    printf("Nome do volume........: %s\n", volume_name[0] ? volume_name : "<sem nome>");
    printf("Tamanho da imagem.....: %lld bytes\n", (long long)image_size);
    printf("Espaço livre..........: %u KiB\n", free_blocks * block_size / 1024);
    printf("Inodes livres.........: %u\n", free_inodes);
    printf("Blocos livres.........: %u\n", free_blocks);
    printf("Tamanho do bloco......: %u bytes\n", block_size);
    printf("Tamanho do inode......: %u bytes\n", inode_size);
    printf("Quantidade de grupos..: %u\n", group_count);
    printf("Blocos por grupo......: %u\n", blocks_per_group);
    printf("Inodes por grupo......: %u\n", inodes_per_group);
    printf("Blocos da tabela inode: %u\n", inodetable_blocks);

    return EXIT_SUCCESS;
}
