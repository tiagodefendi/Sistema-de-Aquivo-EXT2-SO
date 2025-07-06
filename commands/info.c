#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
    (void)argv; // Argumentos não utilizados
    (void)cwd;  // Diretório corrente não utilizado

    if (argc != 1) // Verifica se o número de argumentos é válido
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    char volume_name[17] = {0};                    // Volume name pode não ser NUL-terminated
    memcpy(volume_name, fs->sb.s_volume_name, 16); // Copia o nome do volume do superbloco

    off_t image_size = lseek(fs->fd, 0, SEEK_END); // Obtém o tamanho total da imagem
    if (image_size < 0)                            // Verifica se houve erro ao ler o tamanho da imagem
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t free_blocks = fs->sb.s_free_blocks_count;                                          // Contagem de blocos livres
    uint32_t free_inodes = fs->sb.s_free_inodes_count;                                          // Contagem de inodes livres
    uint32_t block_size = EXT2_BLOCK_SIZE;                                                      // Fixo em 1 KiB neste projeto
    uint32_t inode_size = fs->sb.s_inode_size;                                                  // 128 B
    uint32_t group_count = fs->groups_count;                                                    // Número de grupos de blocos
    uint32_t blocks_per_group = fs->sb.s_blocks_per_group;                                      // Contagem de blocos por grupo
    uint32_t inodes_per_group = fs->sb.s_inodes_per_group;                                      // Contagem de inodes por grupo
    uint32_t inodetable_blocks = (inodes_per_group * inode_size + block_size - 1) / block_size; // Tamanho da tabela de inodes em blocos

    printf("Volume name.....: %s\n", volume_name[0] ? volume_name : "<no name>");
    printf("Image size......: %lld bytes\n", (long long)image_size);
    printf("Free space......: %u KiB\n", ((free_blocks - fs->sb.s_r_blocks_count) * block_size) / 1024);
    printf("Free inodes.....: %u\n", free_inodes);
    printf("Free blocks.....: %u\n", free_blocks);
    printf("Block size......: %u bytes\n", block_size);
    printf("Inode size......: %u bytes\n", inode_size);
    printf("Groups count....: %u\n", group_count);
    printf("Groups size.....: %u blocks\n", blocks_per_group);
    printf("Groups inodes...: %u inodes\n", inodes_per_group);
    printf("Inodetable size.: %u blocks\n", inodetable_blocks);

    return EXIT_SUCCESS;
}
