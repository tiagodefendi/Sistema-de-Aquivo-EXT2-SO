#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "commands.h"

/** * @brief   Lê um bloco do sistema de arquivos EXT2 e escreve seu conteúdo no stdout.
 *
 * Se o número do bloco for zero, escreve zeros. Caso contrário,
 * lê o bloco do sistema de arquivos e escreve seu conteúdo.
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param blk   Número do bloco a ser lido (0 para bloco vazio).
 * @param nbytes Número de bytes a serem escritos.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int dump_blk(ext2_fs_t *fs, uint32_t blk, uint32_t nbytes)
{
    static uint8_t zbuf[EXT2_BLOCK_SIZE] = {0}; // Buffer de zeros para blocos vazios
    uint8_t buf[EXT2_BLOCK_SIZE]; // Buffer para leitura do bloco

    const void *src = zbuf;
    if (blk)
    {
        if (fs_read_block(fs, blk, buf) < 0) // Lê o bloco do sistema de arquivos
            return -1;
        src = buf;
    }
    size_t written = fwrite(src, 1, nbytes, stdout); // Escreve o conteúdo
    if (written != nbytes)
    {
        return -1;
    }
    return 0;
}

/**
 * @brief   Lê o conteúdo de um arquivo regular do sistema de arquivos EXT2 e o escreve no stdout.
 *
 * Lê os blocos do inode do arquivo e escreve seu conteúdo no stdout.
 * Suporta blocos diretos, indiretos simples e indiretos duplos.
 *
 * @param fs   Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param in   Ponteiro para o inode do arquivo a ser lido.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int dump_file(ext2_fs_t *fs, const struct ext2_inode *in)
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
 * @brief   Comando 'cat' para exibir o conteúdo de um arquivo regular no sistema de arquivos EXT2.
 *
 * Este comando recebe o nome de um arquivo e exibe seu conteúdo no stdout.
 * Se o arquivo não for encontrado ou não for um arquivo regular, exibe uma mensagem de erro.
 *
 * @param argc Número de argumentos passados para o comando.
 * @param argv Array de strings contendo os argumentos do comando.
 * @param fs   Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd  Ponteiro para o inode do diretório atual.
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

    char *abs = fs_join_path(fs, *cwd, argv[1]); // Resolve o caminho absoluto do arquivo
    if (!abs)
        return -1;

    uint32_t ino;
    if (fs_path_resolve(fs, abs, &ino) < 0) // Resolve o inode do arquivo
    {
        free(abs);
        return -1;
    }

    struct ext2_inode in;
    if (fs_read_inode(fs, ino, &in) < 0) // Lê o inode do arquivo
    {
        free(abs);
        return -1;
    }
    free(abs);

    if (!ext2_is_reg(&in)) // Verifica se o inode é um arquivo regular
    {
        errno = EISDIR;
        fprintf(stderr, "%s: não é arquivo regular\n", argv[1]);
        return -1;
    }

    if (dump_file(fs, &in) < 0) // Lê o conteúdo do arquivo e escreve no stdout
        return -1;
    putchar('\n');
    return 0;
}
