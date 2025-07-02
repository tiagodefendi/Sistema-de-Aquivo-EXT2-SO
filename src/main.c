/*
 * Shell interativo (ext2shell)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"
#include "utils.h"
#include "commands.h"

#define MAX_LINE 256 /* tamanho máximo de comando */
#define MAX_ARGS 8   /* nº máx de argumentos suportados */

/* remove newline final de fgets() */
static void trim_newline(char *s)
{
    size_t len = strlen(s);
    if (len && s[len - 1] == '\n')
        s[len - 1] = '\0';
}

/* Despacha argv[0] conforme comando solicitado; retorna <0 para sair */
static int dispatch(char **argv, int argc, FILE *img, char *cwd)
{
    if (argc == 0)
        return 0;

    /* comandos de término do shell */
    if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0)
        return -1;

    /* --------- comandos já implementados --------- */
    if (strcmp(argv[0], "info") == 0)
    {
        if (argc != 1)
        {
            fprintf(stderr, "info: invalid syntax.\n");
            return 0;
        }
        if (cmd_info(img) != 0)
            fprintf(stderr, "info: failed.\n");
        return 0;
    }

    /* ----- comando ls ----- */
    if (strcmp(argv[0], "ls") == 0)
    {
        if (argc > 1)
        {
            fprintf(stderr, "ls: invalid syntax.\n");
            return 0;
        }
        cmd_ls(img, cwd);
        return 0;
    }

    /* ---------- cat <filename> ---------- */
    if (strcmp(argv[0], "cat") == 0)
    {
        if (argc != 2)
        {
            fprintf(stderr, "cat: usage: cat <filename>\n");
            return 0;
        }
        if (cmd_cat(img, cwd, argv[1]) != 0)
            fprintf(stderr, "cat: failed.\n");
        return 0;
    }

    /* ---------------- pwd ---------------- */
    if (strcmp(argv[0], "pwd") == 0)
    {
        if (argc != 1)
        {
            fprintf(stderr, "pwd: too many arguments.\n");
            return 0;
        }
        if (cmd_pwd(cwd) != 0)
            fprintf(stderr, "pwd: failed.\n");
        return 0;
    }

    /* ---------------- attr ---------------- */
    if (strcmp(argv[0], "attr") == 0)
    {
        if (argc != 2)
        {
            fprintf(stderr, "attr: usage attr <file|dir>\n");
            return 0;
        }
        if (cmd_attr(img, cwd, argv[1]) != 0)
            fprintf(stderr, "attr: failed\n");
        return 0;
    }

    /* --------------- print --------------- */
    if (strcmp(argv[0], "print") == 0)
    {
        if (cmd_print(img, cwd, argc, argv) != 0)
            fprintf(stderr, "print: failed\n");
        return 0;
    }

    /* ---------------- cd ---------------- */
    if (strcmp(argv[0], "cd") == 0)
    {
        if (argc != 2)
        {
            fprintf(stderr, "cd: usage cd <path>\\n");
            return 0;
        }
        cmd_cd(img, cwd, argv[1]);
        return 0;
    }

    /* ------------- not found ------------- */
    fprintf(stderr, "command not found.\n");
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <ext2_image>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* --- abre imagem para leitura/escrita binária --- */
    FILE *img = fopen(argv[1], "r+b");
    if (!img)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    char cwd[256] = "/"; /* diretório corrente em formato texto */
    char line[MAX_LINE];
    char *args[MAX_ARGS + 1];

    /* --------------- loop do prompt --------------- */
    while (1)
    {
        printf("\033[1;34m[%s]$> \033[0m", cwd);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break; /* EOF / Ctrl+D */

        trim_newline(line);

        /* tokenização simples por espaço/tab */
        int argc_cmd = 0;
        char *tok = strtok(line, " \t");
        while (tok && argc_cmd < MAX_ARGS)
        {
            args[argc_cmd++] = tok;
            tok = strtok(NULL, " \t");
        }
        args[argc_cmd] = NULL;

        if (dispatch(args, argc_cmd, img, cwd) < 0)
            break;
    }

    fclose(img);
    return EXIT_SUCCESS;
}
