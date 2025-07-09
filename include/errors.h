#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>
#include <string.h>

#define ERROR_DIRECTORY_NOT_FOUND 1
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_FILE_OR_DIRECTORY_NOT_FOUND 3
#define ERROR_COMMAND_NOT_FOUND 4
#define ERROR_INVALID_SYNTAX 5
#define ERROR_FILE_OR_DIRECTORY_ALREADY_EXISTS 6
#define ERROR_DIRECTORY_NOT_EMPTY 7
#define ERROR_DEST_DIR_NOT_EXISTS 8
#define ERROR_UNKNOWN 10

#define RED_COLOR "\033[31m"
#define RESET_COLOR "\033[0m"

/**
 * @brief   Imprime uma mensagem de erro com base no código de erro fornecido.
 *
 * Esta função imprime uma mensagem de erro correspondente ao código de erro
 * passado como argumento. As mensagens são exibidas em vermelho.
 *
 * @param error Código de erro a ser impresso.
 */
static inline void print_error(int error)
{
    switch (error)
    {
    case 0:
        return; // Nenhum erro
    case ERROR_DIRECTORY_NOT_FOUND:
        fprintf(stderr, RED_COLOR "diretório não encontrado.\n" RESET_COLOR);
        break;
    case ERROR_FILE_NOT_FOUND:
        fprintf(stderr, RED_COLOR "arquivo não encontrado.\n" RESET_COLOR);
        break;
    case ERROR_FILE_OR_DIRECTORY_NOT_FOUND:
        fprintf(stderr, RED_COLOR "arquivo ou diretório não encontrado.\n" RESET_COLOR);
        break;
    case ERROR_COMMAND_NOT_FOUND:
        fprintf(stderr, RED_COLOR "comando não encontrado.\n" RESET_COLOR);
        break;
    case ERROR_INVALID_SYNTAX:
        fprintf(stderr, RED_COLOR "sintaxe inválida.\n" RESET_COLOR);
        break;
    case ERROR_FILE_OR_DIRECTORY_ALREADY_EXISTS:
        fprintf(stderr, RED_COLOR "arquivo ou diretório já existe.\n" RESET_COLOR);
        break;
    case ERROR_DIRECTORY_NOT_EMPTY:
        fprintf(stderr, RED_COLOR "diretório não está vazio.\n" RESET_COLOR);
        break;
    case ERROR_DEST_DIR_NOT_EXISTS:
        fprintf(stderr, RED_COLOR "diretório de destino não existe.\n" RESET_COLOR);
        break;
    case ERROR_UNKNOWN:
        fprintf(stderr, RED_COLOR "erro inesperado.\n" RESET_COLOR);
        break;
    }
}

/**
 * @brief   Imprime uma mensagem de erro personalizada.
 *
 * Esta função imprime uma mensagem de erro personalizada em vermelho.
 *
 * @param message Mensagem de erro a ser impressa.
 */
static inline void print_error_with_message(char *message)
{
    fprintf(stderr, RED_COLOR "%s\n" RESET_COLOR, message);
}

#endif /* ERRORS_H */