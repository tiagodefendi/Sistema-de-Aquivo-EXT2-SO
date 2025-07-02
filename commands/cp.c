#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "commands.h"

/**
 * @brief   Escreve um bloco de dados no arquivo destino.
 *
 * Se o número do bloco for zero, escreve zeros. Caso contrário,
 * lê o bloco do sistema de arquivos e escreve seu conteúdo.
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param blk   Número do bloco a ser lido (0 para bloco vazio).
 * @param fd    Descritor de arquivo do destino.
 * @param bytes Número de bytes a serem escritos.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
static int dump_block(ext2_fs_t *fs, uint32_t blk, int fd, uint32_t bytes)
{
    static uint8_t zero_block[EXT2_BLOCK_SIZE] = {0};
    uint8_t data_block[EXT2_BLOCK_SIZE];
    const void *data = zero_block;

    if (blk != 0)
    {
        if (fs_read_block(fs, blk, data_block) < 0)
            return -1;
        data = data_block;
    }

    ssize_t written = write(fd, data, bytes);
    return (written == (ssize_t)bytes) ? 0 : -1;
}

/**
 * @brief   Copia o conteúdo de um arquivo regular do sistema de arquivos EXT2 para o host.
 *
 * Lê os blocos do inode do arquivo e escreve seu conteúdo no arquivo destino.
 * Suporta blocos diretos, indiretos simples e indiretos duplos.
 *
 * @param fs   Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param ino  Número do inode do arquivo a ser copiado.
 * @param dst  Caminho do arquivo destino no host.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
static int copy_ext2_to_host(ext2_fs_t *fs, uint32_t ino, char *dst)
{
    struct ext2_inode in;
    if (fs_read_inode(fs, ino, &in) < 0)
        return -1;
    if (!ext2_is_reg(&in))
    {
        errno = EISDIR;
        return -1;
    }

    int fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -1;

    uint32_t bytes_left = in.i_size; // <= 64 MiB
    int result = 0;

    // Blocos diretos (0‑11)
    for (int i = 0; i < 12 && bytes_left; ++i)
    {
        uint32_t n = (bytes_left >= EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left; // Tamanho do bloco a ser escrito
        if (dump_block(fs, in.i_block[i], fd, n) < 0)                                // Lê o bloco do inode e escreve no arquivo destino
        {
            result = -1;
            break;
        }
        bytes_left -= n; // Reduz o número de bytes restantes
    }

    // Bloco indireto simples (12)
    if (result == 0 && bytes_left)
    {
        uint8_t buf[EXT2_BLOCK_SIZE] = {0};
        if (in.i_block[12])
        {
            if (fs_read_block(fs, in.i_block[12], buf) < 0) // Lê o bloco indireto
                result = -1;
        }
        if (result == 0)
        {
            uint32_t *tbl = (uint32_t *)buf;
            for (uint32_t j = 0; j < PTRS_PER_BLOCK && bytes_left; ++j) // Percorre os ponteiros do bloco indireto
            {
                uint32_t n = (bytes_left >= EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left; // Tamanho do bloco a ser escrito
                uint32_t blk = in.i_block[12] ? tbl[j] : 0;                                  // Se não houver bloco indireto, usa 0
                if (dump_block(fs, blk, fd, n) < 0)                                          // Lê o bloco do inode indireto e escreve no arquivo destino
                {
                    result = -1;
                    break;
                }
                bytes_left -= n; // Reduz o número de bytes restantes
            }
        }
    }

    // Bloco indireto duplo (13)
    if (result == 0 && bytes_left)
    {
        if (!in.i_block[13])
        {
            errno = EIO;
            result = -1;
        }
        else
        {
            uint8_t buf1[EXT2_BLOCK_SIZE], buf2[EXT2_BLOCK_SIZE];
            if (fs_read_block(fs, in.i_block[13], buf1) < 0)
            {
                result = -1;
            }
            else
            {
                uint32_t *lvl1 = (uint32_t *)buf1;
                for (uint32_t i1 = 0; i1 < PTRS_PER_BLOCK && bytes_left && result == 0; ++i1) // Percorre os ponteiros do bloco indireto
                {
                    uint32_t l2blk = lvl1[i1];
                    if (l2blk && fs_read_block(fs, l2blk, buf2) < 0) // Lê o bloco indireto duplo
                    {
                        result = -1;
                        break;
                    }
                    uint32_t *lvl2 = (uint32_t *)buf2;
                    for (uint32_t i2 = 0; i2 < PTRS_PER_BLOCK && bytes_left && result == 0; ++i2) // Percorre os ponteiros do bloco indireto duplo
                    {
                        uint32_t blk = l2blk ? lvl2[i2] : 0;                                         // Se não houver bloco indireto duplo, usa 0
                        uint32_t n = (bytes_left >= EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left; // Tamanho do bloco a ser escrito
                        if (dump_block(fs, blk, fd, n) < 0)
                        {
                            result = -1;
                            break;
                        }
                        bytes_left -= n; // Reduz o número de bytes restantes
                    }
                }
            }
        }
    }

    close(fd);
    if (result < 0)
        unlink(dst);
    return result;
}

/**
 * @brief   Resolve o caminho de um arquivo na imagem EXT2.
 *
 * Se o caminho começar com '/', usa-o diretamente. Caso contrário, junta com o diretório atual.
 * Tenta resolver o caminho e retorna o inode correspondente.
 *
 * @param fs         Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd        Inode do diretório atual.
 * @param arg        Caminho relativo ou absoluto do arquivo.
 * @param abs_out    Ponteiro para armazenar o caminho absoluto resolvido.
 * @param ino_out    Ponteiro para armazenar o inode do arquivo, se encontrado.
 *
 * @return Retorna 1 se o caminho foi resolvido com sucesso, 0 se não foi encontrado, ou -1 em caso de erro.
 */
static int resolve_image_path(ext2_fs_t *fs, uint32_t cwd, char *arg, char **abs_out, uint32_t *ino_out)
{
    // Se o caminho começa com '/', já é absoluto; senão, monta o caminho absoluto a partir do cwd
    char *abs_path;
    if (arg[0] == '/')
        abs_path = strdup(arg); // Caminho absoluto fornecido
    else
        abs_path = fs_join_path(fs, cwd, arg); // Caminho relativo, junta com cwd
    if (!abs_path)
        return -1;

    int found = fs_path_resolve(fs, abs_path, ino_out);
    *abs_out = abs_path;

    if (found == 0)
        return 1;
    return 0;
}

/**
 * @brief   Comando para copiar um arquivo do sistema de arquivos EXT2 para o host.
 *
 * Este comando recebe o caminho de um arquivo na imagem EXT2 e o caminho de destino no host,
 * copiando o conteúdo do arquivo da imagem para o sistema de arquivos local.
 *
 * @param argc  Número de argumentos passados.
 * @param argv  Array de argumentos, onde argv[1] é o arquivo na imagem e argv[2] é o destino no host.
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd   Ponteiro para o inode do diretório corrente.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_cp(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 3)
    {
        fprintf(stderr, "Uso: cp <arquivo_na_imagem> <destino_no_host>\n");
        errno = EINVAL;
        return -1;
    }

    char *src_path = NULL, *dst_path = NULL;
    uint32_t src_ino = 0;

    // Resolve o caminho do arquivo de origem na imagem EXT2
    int src_found = resolve_image_path(fs, *cwd, argv[1], &src_path, &src_ino);
    if (src_found < 0)
        return -1;

    // O destino deve ser um caminho no host, não na imagem
    int dst_found = resolve_image_path(fs, *cwd, argv[2], &dst_path, NULL);
    if (dst_found < 0)
    {
        free(src_path);
        return -1;
    }

    // Só permitimos copiar da imagem para o host
    if (!src_found)
    {
        fprintf(stderr, "Arquivo de origem não encontrado na imagem: %s\n", argv[1]);
        free(src_path);
        free(dst_path);
        errno = ENOENT;
        return -1;
    }
    if (dst_found)
    {
        fprintf(stderr, "O destino deve ser um caminho no host, não na imagem EXT2.\n");
        free(src_path);
        free(dst_path);
        errno = ENOTSUP;
        return -1;
    }

    int rc = copy_ext2_to_host(fs, src_ino, dst_path);
    if (rc < 0)
        perror("cp");

    free(src_path);
    free(dst_path);
    return rc;
}
