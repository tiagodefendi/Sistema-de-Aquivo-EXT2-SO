#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

/**
 * @brief   Lista o conteúdo de um diretório no sistema de arquivos EXT2.
 *
 * Esta função percorre os blocos de dados do diretório, imprimindo as entradas
 * encontradas, incluindo o número do inode, nome e tipo de arquivo.
 *
 * @param fs         Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode  Ponteiro para o inode do diretório a ser listado.
 *
 * @return Retorna 0 em caso de sucesso, ou 1 em caso de erro.
 */
static int list_directory(ext2_fs_t *fs, struct ext2_inode *dir_inode)
{
    uint8_t buf[EXT2_BLOCK_SIZE]; // Buffer para armazenar os dados lidos do bloco

    for (int i = 0; i < 12; ++i) // Percorre os blocos diretos do inode do diretório
    {
        uint32_t bloco = dir_inode->i_block[i]; // Obtém o número do bloco
        if (bloco == 0)                         // Se o bloco não estiver alocado, pula para o próximo
            continue;

        if (fs_read_block(fs, bloco, buf) < 0) // Lê o bloco do diretório
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }

        uint32_t offset = 0;             // Inicializa o deslocamento para percorrer o bloco
        while (offset < EXT2_BLOCK_SIZE) // Percorre o bloco até o final
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + offset); // Obtém a entrada de diretório a partir do buffer

            if (entry->rec_len == 0) // Se o comprimento do registro for zero, significa que a entrada está corrompida
                break;

            if (entry->inode != 0) // Se o inode da entrada for diferente de zero, imprime a entrada
            {
                print_entry(entry);
            }
            offset += entry->rec_len; // Atualiza o deslocamento para a próxima entrada
        }
    }
    return EXIT_SUCCESS;
}

/**
 * @brief   Comando 'ls' para listar o conteúdo de um diretório ou arquivo.
 *
 * Este comando aceita um argumento opcional que especifica o caminho a ser listado.
 * Se nenhum argumento for fornecido, lista o diretório atual.
 *
 * @param argc  Número de argumentos passados para o comando.
 * @param argv  Array de strings contendo os argumentos.
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd   Ponteiro para o inode do diretório corrente.
 *
 * @return Retorna 0 em caso de sucesso, ou 1 em caso de erro.
 */
int cmd_ls(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc > 2) // Verifica se o número de argumentos é válido
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    uint32_t alvo_inode;           // Inode do alvo a ser listado
    char *caminho_completo = NULL; // Caminho completo do alvo

    if (argc == 1) // Se nenhum argumento for fornecido, lista o diretório atual
    {
        alvo_inode = *cwd;
    }
    else // Se um argumento for fornecido, resolve o caminho
    {
        caminho_completo = fs_join_path(fs, *cwd, argv[1]); // Junta o caminho atual com o argumento fornecido
        if (!caminho_completo)                              // Verifica se a junção do caminho foi bem-sucedida
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }
        if (fs_path_resolve(fs, caminho_completo, &alvo_inode) < 0) // Resolve o caminho para obter o inode correspondente
        {
            free(caminho_completo);
            print_error(ERROR_DIRECTORY_NOT_FOUND);
            return EXIT_FAILURE;
        }
    }

    struct ext2_inode inode;                       // Estrutura para armazenar o inode do alvo
    if (fs_read_inode(fs, alvo_inode, &inode) < 0) // Lê o inode do alvo
    {
        free(caminho_completo);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    int resultado;           // Variável para armazenar o resultado da listagem
    if (ext2_is_dir(&inode)) // Verifica se o inode é um diretório
    {
        resultado = list_directory(fs, &inode);
    }
    else // Se não for um diretório, imprime erro
    {
        print_error(ERROR_INVALID_SYNTAX);
        resultado = EXIT_FAILURE;
    }

    free(caminho_completo);
    return resultado;
}
