#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

/**
 * @brief   Copia um arquivo do sistema de arquivos EXT2 para o sistema real.
 *
 * Esta função copia o conteúdo de um arquivo especificado pelo inode do EXT2
 * para um arquivo no sistema real, preservando a estrutura de blocos do EXT2.
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param ino Número do inode do arquivo a ser copiado.
 * @param dst Caminho completo do destino onde o arquivo será salvo.
 *
 * @return Retorna 0 em caso de sucesso, ou 1 em caso de erro.
 */
int dump_block(ext2_fs_t *fs, uint32_t blk, FILE *fd, uint32_t bytes)
{
    static unsigned char zero_block[EXT2_BLOCK_SIZE] = {0}; // Bloco de zeros para blocos não alocados
    unsigned char data_block[EXT2_BLOCK_SIZE];              // Buffer para armazenar os dados do bloco lido
    const void *data = zero_block;                          // Inicializa com bloco de zeros

    if (blk != 0) // Se o bloco não for zero (não alocado)
    {
        if (fs_read_block(fs, blk, data_block) < 0) // Lê o bloco do sistema de arquivos
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }
        data = data_block; // Usa o bloco lido como dados
    }
    size_t written = fwrite(data, 1, bytes, fd);             // Escreve os dados no arquivo de destino
    return (written == bytes) ? EXIT_SUCCESS : EXIT_FAILURE; // Retorna 0 se a escrita foi bem-sucedida, 1 se houve erro
}

/**
 * @brief   Copia um arquivo do sistema de arquivos EXT2 para o sistema real.
 *
 * Esta função copia o conteúdo de um arquivo especificado pelo inode do EXT2
 * para um arquivo no sistema real, preservando a estrutura de blocos do EXT2.
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param ino Número do inode do arquivo a ser copiado.
 * @param dst Caminho completo do destino onde o arquivo será salvo.
 *
 * @return Retorna 0 em caso de sucesso, ou 1 em caso de erro.
 */
int copy_ext2_to_host(ext2_fs_t *fs, uint32_t ino, const char *dst)
{
    struct ext2_inode in;                // Cria inode do arquivo
    if (fs_read_inode(fs, ino, &in) < 0) // Lê o inode do arquivo
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (!ext2_is_reg(&in)) // Verifica se é um arquivo regular
    {
        print_error(ERROR_FILE_NOT_FOUND);
        return EXIT_FAILURE;
    }

    FILE *fd = fopen(dst, "wb"); // Abre o arquivo de destino para escrita binária
    if (!fd)                     // Verifica se o arquivo de destino existe e foi aberto corretamente
    {
        print_error(ERROR_DEST_DIR_NOT_EXISTS);
        return EXIT_FAILURE;
    }
    uint32_t bytes_left = in.i_size; // Tamanho do arquivo a ser copiado
    int result = 0;                  // Variável para armazenar o resultado da cópia

    for (int i = 0; i < 12 && bytes_left; ++i) // Blocos diretos (0-11)
    {
        uint32_t n = (bytes_left >= EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left; // Tamanho do bloco a ser copiado
        if (dump_block(fs, in.i_block[i], fd, n))                                    // Copia o bloco para o arquivo de destino
        {
            print_error(ERROR_UNKNOWN);
            result = EXIT_FAILURE;
            break;
        }
        bytes_left -= n;
    }

    if (result == 0 && bytes_left) // Bloco indireto simples (12)
    {
        unsigned char buf[EXT2_BLOCK_SIZE] = {0}; // Buffer para armazenar os blocos indiretos
        if (in.i_block[12])                       // Verifica se o bloco indireto simples está alocado
        {
            if (fs_read_block(fs, in.i_block[12], buf) < 0)
            {
                print_error(ERROR_UNKNOWN);
                result = EXIT_FAILURE;
            }
        }
        if (result == 0) // Se não houve erro ao ler o bloco indireto simples
        {
            uint32_t *tbl = (uint32_t *)buf;                            // Converte o buffer para um ponteiro de tabela de blocos
            for (uint32_t j = 0; j < PTRS_PER_BLOCK && bytes_left; ++j) // Percorre os ponteiros na tabela de blocos indiretos
            {
                uint32_t n = (bytes_left >= EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left; // Tamanho do bloco a ser copiado
                uint32_t blk = in.i_block[12] ? tbl[j] : 0;                                  // Obtém o bloco a ser copiado

                if (dump_block(fs, blk, fd, n)) // Copia o bloco para o arquivo de destino
                {
                    print_error(ERROR_UNKNOWN);
                    result = EXIT_FAILURE;
                    break;
                }
                bytes_left -= n; // Atualiza o tamanho restante do arquivo a ser copiado
            }
        }
    }

    if (result == 0 && bytes_left) // Bloco indireto duplo (13)
    {
        if (!in.i_block[13]) // Se não houver bloco indireto duplo, não há mais blocos a serem copiados
        {
            print_error(ERROR_UNKNOWN);
            result = EXIT_FAILURE;
        }
        else // Se houver bloco indireto duplo, lê os blocos indiretos
        {
            unsigned char buf1[EXT2_BLOCK_SIZE], buf2[EXT2_BLOCK_SIZE]; // Buffers para armazenar os blocos indiretos
            if (fs_read_block(fs, in.i_block[13], buf1) < 0)            // Lê o bloco indireto duplo
            {
                print_error(ERROR_UNKNOWN);
                result = EXIT_FAILURE;
            }
            else // Se não houve erro ao ler o bloco indireto duplo
            {
                uint32_t *lvl1 = (uint32_t *)buf1; // Converte o buffer para um ponteiro de tabela de blocos indiretos duplos

                for (uint32_t i1 = 0; i1 < PTRS_PER_BLOCK && bytes_left && result == 0; ++i1) // Percorre os ponteiros na tabela de blocos indiretos duplos
                {
                    uint32_t l2blk = lvl1[i1];
                    if (l2blk && fs_read_block(fs, l2blk, buf2) < 0)
                    {
                        print_error(ERROR_UNKNOWN);
                        result = EXIT_FAILURE;
                        break;
                    }
                    uint32_t *lvl2 = (uint32_t *)buf2;
                    for (uint32_t i2 = 0; i2 < PTRS_PER_BLOCK && bytes_left && result == 0; ++i2)
                    {
                        uint32_t blk = l2blk ? lvl2[i2] : 0;
                        uint32_t n = (bytes_left >= EXT2_BLOCK_SIZE) ? EXT2_BLOCK_SIZE : bytes_left;

                        if (dump_block(fs, blk, fd, n)) // Copia o bloco para o arquivo de destino
                        {
                            print_error(ERROR_UNKNOWN);
                            result = EXIT_FAILURE;
                            break;
                        }
                        bytes_left -= n; // Atualiza o tamanho restante do arquivo a ser copiado
                    }
                }
            }
        }
    }

    fclose(fd);
    if (result) // Se houve erro durante a cópia, remove o arquivo de destino
        remove(dst);
    return result; // Retorna 0 se a cópia foi bem-sucedida, 1 se houve erro
}

/**
 * @brief   Resolve o caminho absoluto de um arquivo ou diretório no sistema de arquivos EXT2.
 *
 * Esta função recebe um caminho relativo ou absoluto e resolve para um caminho absoluto,
 * retornando o inode correspondente ao caminho resolvido.
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Inode do diretório corrente.
 * @param arg Caminho relativo ou absoluto a ser resolvido.
 * @param abs_out Ponteiro para armazenar o caminho absoluto resultante.
 * @param ino_out Ponteiro para armazenar o inode correspondente ao caminho resolvido.
 *
 * @return Retorna 0 se o caminho foi resolvido com sucesso, ou 1 se houve erro.
 */
int resolve_image_path(ext2_fs_t *fs, uint32_t cwd, const char *arg, char **abs_out, uint32_t *ino_out)
{
    // Declara um ponteiro para armazenar o caminho absoluto
    char *abs_path;

    if (arg[0] == '/')          // Se o argumento já começa com '/', é um caminho absoluto
        abs_path = strdup(arg); // Faz uma cópia do caminho absoluto
    else                        // Caso contrário, constrói o caminho absoluto a partir do diretório atual (cwd)
        abs_path = fs_join_path(fs, cwd, arg);

    if (!abs_path) // Se não foi possível obter o caminho absoluto, retorna erro
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    int found = fs_path_resolve(fs, abs_path, ino_out); // Resolve o caminho absoluto para obter o número do inode correspondente
    *abs_out = abs_path;                                // Retorna o caminho absoluto pelo ponteiro de saída

    return found == 0 ? EXIT_SUCCESS : EXIT_FAILURE; // Retorna EXIT_SUCCESS se encontrou, EXIT_FAILURE caso contrário
}

/**
 * @brief   Cria o caminho completo de destino para a cópia de arquivos.
 *
 * Esta função constrói o caminho completo de destino, adicionando o nome do arquivo
 * de origem ao diretório de destino, garantindo que o caminho seja válido.
 *
 * @param dst_full Buffer onde o caminho completo de destino será armazenado.
 * @param dst Caminho do diretório de destino.
 * @param src_path Caminho absoluto do arquivo de origem.
 */
void make_dst_path(char *dst_full, const char *dst, const char *src_path)
{
    size_t len = strlen(dst); // Obtém o comprimento da string de destino

    if (dst[len - 1] != '/') // Se o destino NÃO termina com '/'...
    {
        const char *last_slash = strrchr(dst, '/'); // Procura a última barra '/' no destino
        const char *last_dot = strrchr(dst, '.');   // Procura o último ponto '.' no destino

        if (!last_dot || (last_slash && last_dot < last_slash)) // Se NÃO há ponto após a última barra, considera como diretório
        {
            snprintf(dst_full, 4096, "%s/", dst); // Adiciona '/' ao final do destino e armazena em dst_full
            dst = dst_full;                       // Atualiza dst para apontar para dst_full (agora com '/')
            len = strlen(dst);                    // Atualiza o comprimento do novo destino
        }
    }

    if (dst[len - 1] == '/') // Se o destino termina com '/', é um diretório
    {

        const char *base = strrchr(src_path, '/'); // Obtém o nome do arquivo base de src_path (após a última '/')
        base = base ? base + 1 : src_path;         // Se base for NULL, usa src_path inteiro como base
        strcpy(dst_full, dst);                     // Copia o destino (com '/') para dst_full
        strcat(dst_full, base);                    // Adiciona o nome do arquivo base ao destino
    }
    else // Caso contrário, apenas copia o destino para dst_full
    {
        strcpy(dst_full, dst);
    }
}

/**
 * @brief   Comando para copiar um arquivo do sistema de arquivos EXT2 para o sistema real.
 *
 * Este comando deve receber dois argumentos: o caminho do arquivo de origem no EXT2
 * e o caminho absoluto de destino no sistema real.
 *
 * @param argc Número de argumentos (deve ser 3).
 * @param argv Vetor de argumentos, onde argv[1] é o arquivo de origem e argv[2] é o destino.
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o diretório corrente (não utilizado).
 *
 * @return Retorna 0 em caso de sucesso, ou 1 em caso de erro.
 */
int cmd_cp(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 3) // Verifica se o número de argumentos é válido
    {
        print_error(ERROR_INVALID_SYNTAX);
        return 1;
    }

    if (argv[2][0] != '/') // Destino deve ser caminho absoluto do sistema real
    {
        print_error(ERROR_INVALID_SYNTAX);
        return 1;
    }

    char *src_path = NULL;                                          // Caminho absoluto do arquivo de origem
    uint32_t src_ino = 0;                                           // Inode do arquivo de origem
    if (resolve_image_path(fs, *cwd, argv[1], &src_path, &src_ino)) // Resolve o caminho do arquivo de origem
    {
        print_error(ERROR_FILE_NOT_FOUND);
        free(src_path);
        return EXIT_FAILURE;
    }

    char dst_full[4096];                        // Caminho completo do destino
    make_dst_path(dst_full, argv[2], src_path); // Cria o caminho completo do destino

    int res = copy_ext2_to_host(fs, src_ino, dst_full); // Copia o arquivo do EXT2 para o sistema real

    free(src_path);

    return res ? EXIT_FAILURE : EXIT_SUCCESS;
}
