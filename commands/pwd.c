#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
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
    // Ignora argumentos além do comando
    (void)argv;

    // Verifica se o comando foi chamado corretamente
    if (argc != 1)
    {
        fprintf(stderr, "Uso correto: pwd\n");
        errno = EINVAL;
        return -1;
    }

    // Obtém o caminho absoluto do diretório atual
    char *current_path = fs_get_path(fs, *cwd);
    if (!current_path)
    {
        fprintf(stderr, "Erro ao obter o caminho do diretório atual.\n");
        return -1;
    }

    // Imprime o caminho
    printf("%s\n", current_path);

    // Libera a memória alocada para o caminho
    free(current_path);
    return 0;
}
