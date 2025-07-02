#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "commands.h"

/**
 * @brief   Despeja o conteúdo de um arquivo regular para a saída padrão.
 *
 * Esta função lê os blocos de dados do inode do arquivo e escreve seu conteúdo na saída padrão.
 * Suporta blocos diretos e um bloco indireto.
 *
 * @param fs     Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param inode  Ponteiro para o inode do arquivo a ser lido.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se ocorrer falha na leitura).
 */
static int dump_file(ext2_fs_t *fs, struct ext2_inode *inode)
{
    uint32_t bytes_left = inode->i_size;
    int result = 0;

    // Blocos diretos (0-11)
    for (int i = 0; i < 12 && bytes_left; ++i)
    {
        uint8_t buf[EXT2_BLOCK_SIZE];
        uint32_t blk = inode->i_block[i];
        uint32_t n = (bytes_left >= EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left;
        if (blk != 0) {
            if (fs_read_block(fs, blk, buf) < 0) {
                result = -1;
                break;
            }
            if (fwrite(buf, 1, n, stdout) != n) {
                result = -1;
                break;
            }
        } else {
            uint8_t zero[EXT2_BLOCK_SIZE] = {0};
            if (fwrite(zero, 1, n, stdout) != n) {
                result = -1;
                break;
            }
        }
        bytes_left -= n;
    }

    // Bloco indireto simples (12)
    if (result == 0 && bytes_left)
    {
        uint8_t buf[EXT2_BLOCK_SIZE] = {0};
        if (inode->i_block[12]) {
            if (fs_read_block(fs, inode->i_block[12], buf) < 0)
                result = -1;
        }
        uint32_t *tbl = (uint32_t *)buf;
        for (uint32_t j = 0; j < 256 && bytes_left && result == 0; ++j)
        {
            uint32_t n = (bytes_left >= EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left;
            uint32_t blk = inode->i_block[12] ? tbl[j] : 0;
            if (blk != 0) {
                uint8_t data[EXT2_BLOCK_SIZE];
                if (fs_read_block(fs, blk, data) < 0) {
                    result = -1;
                    break;
                }
                if (fwrite(data, 1, n, stdout) != n) {
                    result = -1;
                    break;
                }
            } else {
                uint8_t zero[EXT2_BLOCK_SIZE] = {0};
                if (fwrite(zero, 1, n, stdout) != n) {
                    result = -1;
                    break;
                }
            }
            bytes_left -= n;
        }
    }

    // Bloco indireto duplo (13)
    if (result == 0 && bytes_left)
    {
        if (!inode->i_block[13]) {
            errno = EIO;
            result = -1;
        } else {
            uint8_t buf1[EXT2_BLOCK_SIZE], buf2[EXT2_BLOCK_SIZE];
            if (fs_read_block(fs, inode->i_block[13], buf1) < 0) {
                result = -1;
            } else {
                uint32_t *lvl1 = (uint32_t *)buf1;
                for (uint32_t i1 = 0; i1 < 256 && bytes_left && result == 0; ++i1) {
                    uint32_t l2blk = lvl1[i1];
                    if (l2blk && fs_read_block(fs, l2blk, buf2) < 0) {
                        result = -1;
                        break;
                    }
                    uint32_t *lvl2 = (uint32_t *)buf2;
                    for (uint32_t i2 = 0; i2 < 256 && bytes_left && result == 0; ++i2) {
                        uint32_t blk = l2blk ? lvl2[i2] : 0;
                        uint32_t n = (bytes_left >= EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left;
                        if (blk != 0) {
                            uint8_t data[EXT2_BLOCK_SIZE];
                            if (fs_read_block(fs, blk, data) < 0) {
                                result = -1;
                                break;
                            }
                            if (fwrite(data, 1, n, stdout) != n) {
                                result = -1;
                                break;
                            }
                        } else {
                            uint8_t zero[EXT2_BLOCK_SIZE] = {0};
                            if (fwrite(zero, 1, n, stdout) != n) {
                                result = -1;
                                break;
                            }
                        }
                        bytes_left -= n;
                    }
                }
            }
        }
    }

    return result;
}

/**
 * @brief   Comando para exibir o conteúdo de um arquivo regular.
 *
 * Este comando recebe o nome de um arquivo e exibe seu conteúdo na saída padrão.
 * Se o arquivo não for regular, retorna um erro.
 *
 * @param argc  Número de argumentos passados para o comando.
 * @param argv  Array de strings contendo os argumentos do comando.
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd   Ponteiro para o inode do diretório atual.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_cat(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: cat <arquivo>\n");
        errno = EINVAL;
        return -1;
    }

    // Monta o caminho absoluto do arquivo
    char *caminho_completo = fs_join_path(fs, *cwd, argv[1]);
    if (!caminho_completo)
        return -1;

    // Resolve o caminho para obter o número do inode
    uint32_t inode_num;
    if (fs_path_resolve(fs, caminho_completo, &inode_num) < 0)
    {
        free(caminho_completo);
        return -1;
    }

    // Lê o inode do arquivo
    struct ext2_inode inode;
    if (fs_read_inode(fs, inode_num, &inode) < 0)
    {
        free(caminho_completo);
        return -1;
    }

    // Verifica se é um arquivo regular
    if (!ext2_is_reg(&inode))
    {
        fprintf(stderr, "%s: não é um arquivo regular\n", argv[1]);
        free(caminho_completo);
        errno = EISDIR;
        return -1;
    }

    // Exibe o conteúdo do arquivo
    int resultado = dump_file(fs, &inode);
    putchar('\n');
    free(caminho_completo);
    return resultado;
}
