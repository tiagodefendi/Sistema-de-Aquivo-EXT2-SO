#include <stdio.h>
#include <stdlib.h>

#include "commands.h"

/**
 * @brief Imprime o caminho do diretório atual.
 *
 * Esta função imprime o caminho completo do diretório atual no sistema de arquivos.
 *
 * @param argc Número de argumentos passados para o comando.
 * @param argv Array de strings contendo os argumentos do comando.
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o número do inode do diretório atual.
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_pwd(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    (void)argv; // Ignora argumentos além do comando

    if (argc != 1) // Verifica se o comando foi chamado corretamente
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    char *current_path = fs_get_path(fs, *cwd); // Obtém o caminho absoluto do diretório atual
    if (!current_path)                          // Verifica se a obtenção do caminho foi bem-sucedida
    {
        print_error(ERROR_DIRECTORY_NOT_FOUND);
        return EXIT_FAILURE;
    }

    printf("%s\n", current_path); // Imprime o caminho
    free(current_path);           // Libera a memória alocada para o caminho
    return EXIT_SUCCESS;
}
