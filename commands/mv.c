#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"

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
static int resolve_image_path(ext2_fs_t *fs, uint32_t cwd, char *arg, char **abs_out, uint32_t *ino_out)
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
 * @brief   Comando para mover um arquivo do sistema de arquivos EXT2 para o host.
 *
 * Este comando recebe o caminho de um arquivo na imagem EXT2 e o caminho de destino no host,
 * copiando o conteúdo do arquivo da imagem para o sistema de arquivos local e removendo-o da imagem.
 *
 * @param argc  Número de argumentos passados.
 * @param argv  Array de argumentos, onde argv[1] é o arquivo na imagem e argv[2] é o destino no host.
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd   Ponteiro para o inode do diretório corrente.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_mv(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 3) // Verifica se o número de argumentos é válido
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    if (argv[2][0] != '/') // Destino deve ser caminho absoluto do sistema real
    {
        print_error(ERROR_DEST_DIR_NOT_EXISTS);
        return EXIT_FAILURE;
    }

    char *src_path = NULL;                                          // Caminho absoluto do arquivo de origem
    uint32_t src_ino = 0;                                           // Inode do arquivo de origem
    if (resolve_image_path(fs, *cwd, argv[1], &src_path, &src_ino)) // Resolve o caminho do arquivo de origem
    {
        print_error(ERROR_FILE_NOT_FOUND);
        free(src_path);
        return EXIT_FAILURE;
    }

    if (cmd_cp(argc, argv, fs, cwd))
    {
        return EXIT_FAILURE;
    }
    char *rm_argv[] = {"rm", src_path};
    if (cmd_rm(2, rm_argv, fs, cwd) < 0)
    {
        print_error_with_message("AVISO: arquivo copiado para o destino, mas não foi possível removê-lo da imagem EXT2");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
