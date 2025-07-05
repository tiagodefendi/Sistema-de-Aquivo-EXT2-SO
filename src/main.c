#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "commands.h"
#include "errors.h"

/**
 * @file    main.c
 *
 * Implementa um shell simples para interagir com um sistema de arquivos ext2.
 *
 * @authors Artur Bento de Carvalho
 *          Eduardo Riki Matushita
 *          Rafaela Tieri Iwamoto Ferreira
 *          Tiago Defendi da Silva
 *
 * @date    01/07/2025
 */

#define MAX_TOKENS 32
#define MAX_BUFFER_SHELL 1024

/**
 * @brief   Tokeniza uma linha de comando em argumentos.
 *
 * Esta função divide uma linha de comando em tokens, considerando espaços,
 * tabulações e novas linhas como delimitadores. Suporta tokens entre aspas.
 *
 * @param line Linha de comando a ser tokenizada.
 * @param argv Array onde os tokens serão armazenados.
 *
 * @return Número de tokens encontrados.
 */
int tokenize(char *line, char **argv)
{
    int argc = 0;
    char *p = line;

    while (*p)
    {
        // Pula espaços fora de aspas
        while (*p == ' ' || *p == '\t' || *p == '\n')
            ++p;
        if (!*p)
            break;

        // início do token
        char *token = p;
        char quote = 0;

        if (*p == '\"' || *p == '\'')
        { // Token entre aspas
            quote = *p++;
            token = p; // Conteúdo sem aspas
            while (*p && *p != quote)
                ++p;
        }
        else
        { // Token sem aspas
            while (*p && *p != ' ' && *p != '\t' && *p != '\n')
                ++p;
        }

        if (*p)
            *p++ = '\0'; /* Termina token */

        argv[argc++] = token;
        if (argc >= MAX_TOKENS - 1)
            break; // Evita overflow
    }
    argv[argc] = NULL;
    return argc;
}

/**
 * @brief   Imprime uma mensagem de erro genérica.
 * @param   msg Mensagem a ser exibida.
 * @return  Nenhum valor é retornado.
 */
void print_errno(const char *msg)
{
    fprintf(stderr, "%s\n", strerror(errno));
}

// Tabela de comandos
struct command_entry cmd_table[] = {
    {"info", cmd_info, "Exibe informações do disco e do sistema de arquivos."},
    {"ls", cmd_ls, "Lista os arquivos e diretórios do diretório corrente."},
    {"cd", cmd_cd, "Altera o diretório corrente para o definido como <path>."},
    {"pwd", cmd_pwd, "Exibe o diretório corrente (caminho absoluto)."},
    {"cat", cmd_cat, "Exibe o conteúdo de um arquivo <file> no formato texto."},
    {"attr", cmd_attr, "Exibe os atributos de um arquivo (<file>) ou diretório (<dir>)."},
    {"touch", cmd_touch, "Cria o arquivo <file> com conteúdo vazio."},
    {"mkdir", cmd_mkdir, "Cria o diretório <dir> vazio."},
    {"rm", cmd_rm, "Remove o arquivo <file> do sistema."},
    {"rmdir", cmd_rmdir, "Remove o diretório <dir>, se estiver vazio."},
    {"rename", cmd_rename, "Renomeia arquivo <file> para <newfilename>."},
    {"cp", cmd_cp, "Copia um arquivo de origem (<source_path>) para destino (<target_path>)."},
    {"mv", cmd_mv, "Move um arquivo da imagem EXT2 para o host (remove após copiar)."},
    {"print", cmd_print, "Exibe informações do sistema EXT2."},
    CMD_TABLE_END};

/**
 * @brief   Mostra a ajuda do shell.
 * @param   Nenhum parâmetro é necessário.
 * @return  Nenhum valor é retornado.
 */
void mostrar_help()
{
    printf("Comandos disponíveis:\n");
    for (struct command_entry *ce = cmd_table; ce->name; ++ce)
        printf("  %-8s - %s\n", ce->name, ce->help ? ce->help : "(sem ajuda)");
    puts("  help     - Exibe todos os comandos disponíveis.");
    puts("  exit     - Finaliza o shell.");
}

/**
 * @brief   Função principal do shell.
 *
 * Esta função inicializa o sistema de arquivos, lê os comandos do usuário e os executa.
 *
 * @param   argc Número de argumentos da linha de comando.
 * @param   argv Array de strings com os argumentos da linha de comando.
 *
 * @return  Número inteiro de sucesso (0) ou falha (1).
 */
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <imagem.ext2>\n", argv[0]);
        return EXIT_FAILURE;
    }

    ext2_fs_t *fs = fs_open(argv[1]); // Abre a imagem do sistema de arquivos
    if (!fs)
    {
        print_errno("Erro ao abrir a imagem do sistema de arquivos.");
        return EXIT_FAILURE;
    }

    uint32_t cwd = EXT2_ROOT_INO; // Diretório corrente
    char line[MAX_BUFFER_SHELL];

    while (1)
    {
        char *pwd = fs_get_path(fs, cwd);        // Caminho atual
        printf("\033[1;34m[%s]\033[0m$> ", pwd); // [Diretório]$>
        free(pwd);
        fflush(stdout); // Garante que o prompt seja exibido antes de ler a entrada

        // Lê a linha de comando do usuário
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            putchar('\n');
            break; /* EOF ou erro */
        }

        char *argvv[MAX_TOKENS]; // Array para armazenar os argumentos tokenizados
        int argc_cmd = tokenize(line, argvv);
        if (argc_cmd == 0)
            continue; // Linha vazia

        if (strcmp(argvv[0], "exit") == 0 || strcmp(argvv[0], "quit") == 0)
        {
            printf("\nSaindo...\n\n");
            break;
        }
        if (strcmp(argvv[0], "help") == 0)
        {
            mostrar_help();
            continue;
        }

        struct command_entry *cmd = NULL;
        // Procura o comando digitado na tabela de comandos
        for (struct command_entry *ce = cmd_table; ce->name; ++ce)
        {
            if (strcmp(ce->name, argvv[0]) == 0)
            {
                cmd = ce;
                break;
            }
        }
        if (!cmd)
        {
            fprintf(stderr, "Comando desconhecido.\n");
            continue;
        }

        cmd->handler(argc_cmd, argvv, fs, &cwd);
    }

    // Fecha o sistema de arquivos
    fs_close(fs);
    return EXIT_SUCCESS;
}
