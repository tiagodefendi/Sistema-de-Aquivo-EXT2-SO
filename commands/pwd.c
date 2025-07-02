/*
 * Imprime o diretório corrente como caminho absoluto.
 */

#include <stdio.h>
#include <string.h>

/*
 * cmd_pwd(cwd)
 *   Exibe cwd e retorna 0.
 */
int cmd_pwd(const char *cwd)
{
    if (!cwd)
    {
        fprintf(stderr, "pwd: internal error (cwd == NULL)\n");
        return -1;
    }
    printf("%s\n", cwd);
    return 0;
}
