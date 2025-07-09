#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 * @return Retorna 0 em caso de sucesso, ou 1 em caso de erro.
 */
int dump_blk(ext2_fs_t *fs, uint32_t blk, uint32_t nbytes)
{
    static uint8_t zbuf[EXT2_BLOCK_SIZE] = {0}; // Buffer de zeros para blocos vazios
    uint8_t buf[EXT2_BLOCK_SIZE];               // Buffer para leitura do bloco

    const void *src = zbuf;
    if (blk)
    {
        if (fs_read_block(fs, blk, buf) < 0) // Lê o bloco do sistema de arquivos
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }
        src = buf;
    }
    size_t written = fwrite(src, 1, nbytes, stdout); // Escreve o conteúdo
    if (written != nbytes)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
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
 * @return Retorna 0 em caso de sucesso, ou 1 em caso de erro.
 */
int dump_file(ext2_fs_t *fs, const struct ext2_inode *in)
{
    uint32_t bytes_left = in->i_size;

    // Blocos diretos
    for (int i = 0; i < 12 && bytes_left > 0; ++i)
    {
        uint32_t to_read = (bytes_left < EXT2_BLOCK_SIZE) ? bytes_left : EXT2_BLOCK_SIZE; // Tamanho a ser lido
        if (dump_blk(fs, in->i_block[i], to_read))
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        } // Lê o bloco do sistema de arquivos
        bytes_left -= to_read;
    }

    // Bloco indireto simples
    if (bytes_left > 0 && in->i_block[12])
    {
        uint8_t buf[EXT2_BLOCK_SIZE];                    // Buffer para leitura do bloco indireto
        if (fs_read_block(fs, in->i_block[12], buf) < 0) // Lê o bloco indireto simples
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }
        uint32_t *indirect = (uint32_t *)buf;
        for (uint32_t i = 0; i < PTRS_PER_BLOCK && bytes_left > 0; ++i) // Lê os blocos indiretos
        {
            uint32_t to_read = (bytes_left > EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left; // Tamanho a ser lido

            if (dump_blk(fs, indirect[i], to_read)) // Lê o bloco indireto
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
            bytes_left -= to_read;
        }
    }

    // Bloco indireto duplo
    if (bytes_left > 0 && in->i_block[13])
    {
        uint8_t buf1[EXT2_BLOCK_SIZE], buf2[EXT2_BLOCK_SIZE]; // Buffers para leitura
        if (fs_read_block(fs, in->i_block[13], buf1) < 0)
            return EXIT_FAILURE;
        uint32_t *dbl_indirect = (uint32_t *)buf1;

        for (uint32_t i = 0; i < PTRS_PER_BLOCK && bytes_left > 0; ++i) // Lê os blocos indiretos duplos
        {
            if (!dbl_indirect[i]) // Verifica se o ponteiro é nulo
                continue;
            if (fs_read_block(fs, dbl_indirect[i], buf2) < 0) // Lê o bloco indireto duplo
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
            uint32_t *indirect = (uint32_t *)buf2;
            for (uint32_t j = 0; j < PTRS_PER_BLOCK && bytes_left > 0; ++j) // Lê os blocos indiretos simples dentro do indireto duplo
            {
                uint32_t to_read = (bytes_left > EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left; // Tamanho a ser lido
                // Lê o bloco indireto simples
                if (dump_blk(fs, indirect[j], to_read) < 0)
                {
                    print_error(ERROR_UNKNOWN);
                    return EXIT_FAILURE;
                }
                bytes_left -= to_read; // Atualiza o número de bytes restantes
            }
        }
    }

    return EXIT_SUCCESS;
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
 * @return Retorna 0 em caso de sucesso, ou 1 em caso de erro.
 */
int cmd_cat(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2)
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    char *abs = fs_join_path(fs, *cwd, argv[1]); // Resolve o caminho absoluto do arquivo
    if (!abs)                                    // Verifica se a junção do caminho foi bem-sucedida
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t ino;
    if (fs_path_resolve(fs, abs, &ino) < 0) // Resolve o inode do arquivo
    {
        free(abs);
        print_error(ERROR_FILE_NOT_FOUND);
        return EXIT_FAILURE;
    }

    struct ext2_inode in;                // Estrutura para armazenar o inode do arquivo
    if (fs_read_inode(fs, ino, &in) < 0) // Lê o inode do arquivo
    {
        print_error(ERROR_UNKNOWN);
        free(abs);
        return EXIT_FAILURE;
    }
    free(abs);

    if (!ext2_is_reg(&in)) // Verifica se o inode é um arquivo regular
    {
        print_error(ERROR_FILE_NOT_FOUND);
        return EXIT_FAILURE;
    }

    if (dump_file(fs, &in)) // Lê o conteúdo do arquivo e escreve no stdout
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    putchar('\n');
    return EXIT_SUCCESS;
}
