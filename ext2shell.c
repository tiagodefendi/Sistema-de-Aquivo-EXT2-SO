#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ext2_utils.h"
#include "commands/info.h"
#include "commands/ls.h"
#include "commands/pwd.h"
#include "commands/cd.h"
#include "commands/cat.h"
#include "commands/attr.h"
#include "commands/touch.h"
#include "commands/mkdir.h"
#include "commands/rm.h"
#include "commands/rmdir.h"
#include "commands/rename.h"
#include "commands/cp.h"
#include "commands/mv.h"

#define CMD_BUFFER_SIZE 128

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <imagem.ext2>\n", argv[0]);
        return 1;
    }

    if (load_image(argv[1]) != 0)
    {
        fprintf(stderr, "Erro ao carregar a imagem.\n");
        return 1;
    }

    reset_path();

    char cmd[CMD_BUFFER_SIZE];
    printf("[%s]$> ", get_current_path());

    while (fgets(cmd, sizeof(cmd), stdin))
    {
        cmd[strcspn(cmd, "\n")] = 0;

        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0)
        {
            break;
        }
        else if (strcmp(cmd, "info") == 0)
        {
            cmd_info();
        }
        else if (strcmp(cmd, "ls") == 0)
        {
            cmd_ls();
        }
        else if (strcmp(cmd, "pwd") == 0)
        {
            cmd_pwd();
        }
        else if (strncmp(cmd, "cd ", 3) == 0)
        {
            char *arg = cmd + 3;
            cmd_cd(arg);
        }
        else if (strncmp(cmd, "cat ", 4) == 0)
        {
            char *arg = cmd + 4;
            cmd_cat(arg);
        }
        else if (strncmp(cmd, "attr ", 5) == 0)
        {
            char *arg = cmd + 5;
            cmd_attr(arg);
        }
        else if (strncmp(cmd, "touch ", 6) == 0)
        {
            cmd_touch(cmd + 6);
        }
        else if (strncmp(cmd, "mkdir ", 6) == 0)
        {
            cmd_mkdir(cmd + 6);
        }
        else if (strncmp(cmd, "rm ", 3) == 0)
        {
            cmd_rm(cmd + 3);
        }
        else if (strncmp(cmd, "rmdir ", 6) == 0)
        {
            cmd_rmdir(cmd + 6);
        }
        else if (strncmp(cmd, "rename ", 7) == 0)
        {
            char *args = cmd + 7;
            char *oldname = strtok(args, " ");
            char *newname = strtok(NULL, " ");
            if (oldname && newname)
            {
                cmd_rename(oldname, newname);
            }
            else
            {
                printf("Uso: rename <nome_antigo> <novo_nome>\n");
            }
        }
        else if (strncmp(cmd, "cp ", 3) == 0)
        {
            char *args = cmd + 3;
            char *src = strtok(args, " ");
            char *dst = strtok(NULL, " ");
            if (src && dst)
            {
                cmd_cp(src, dst);
            }
            else
            {
                printf("Uso: cp <arquivo_na_imagem> <caminho_destino_na_imagem>\n");
            }
        }
        else if (strncmp(cmd, "mv ", 3) == 0)
        {
            char *args = cmd + 3;
            char *src = strtok(args, " ");
            char *dst = strtok(NULL, " ");
            if (src && dst)
            {
                cmd_mv(src, dst);
            }
            else
            {
                printf("Uso: mv <arquivo> <destino>\n");
            }
        }
        else
        {
            printf("Comando não reconhecido: %s\n", cmd);
        }

        printf("[%s]$> ", get_current_path());
    }

    close_image();
    return 0;
}
