#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

/**
 * @brief Renomeia uma entrada de diretório no sistema de arquivos EXT2.
 *
 * Esta função procura uma entrada de diretório com o inode especificado e renomeia-a
 * para o novo nome fornecido. Se a entrada não for encontrada, retorna -1.
 *
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param blk Bloco onde a entrada de diretório está localizada.
 * @param target_ino Número do inode da entrada a ser renomeada.
 * @param newname Novo nome para a entrada de diretório.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
static int rename_entry_block(ext2_fs_t *fs, uint32_t blk, uint32_t target_ino, char *newname)
{
    uint8_t buf[EXT2_BLOCK_SIZE];        // Buffer para armazenar o bloco lido
    if (fs_read_block(fs, blk, buf) < 0) // Lê o bloco do diretório
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t pos = 0;             // Posição atual no buffer
    while (pos < EXT2_BLOCK_SIZE) // Percorre o bloco até o final
    {
        struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + pos); // Obtém a entrada de diretório atual

        if (entry->rec_len == 0) // Se a entrada não tiver comprimento, significa que não há mais entradas
            break;

        if (entry->inode == target_ino) // Verifica se a entrada corresponde ao inode alvo
        {
            uint8_t newname_len = (uint8_t)strlen(newname);      // Comprimento do novo nome
            uint16_t required_len = rec_len_needed(newname_len); // Calcula o comprimento necessário para a nova entrada

            if (required_len > entry->rec_len) // Verifica se o comprimento necessário é maior que o comprimento da entrada atual
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE; // Se não houver espaço suficiente, retorna erro
            }

            entry->name_len = newname_len;             // Atualiza o comprimento do nome da entrada
            memcpy(entry->name, newname, newname_len); // Copia o novo nome para a entrada

            if (newname_len < entry->rec_len - 8)                                       // Se o novo nome for menor que o espaço restante na entrada
                memset(entry->name + newname_len, 0, entry->rec_len - 8 - newname_len); // Preenche com zeros após o novo nome

            return fs_write_block(fs, blk, buf); // Salva o bloco modificado
        }

        pos += entry->rec_len; // Avança para a próxima entrada
    }

    print_error(ERROR_UNKNOWN);
    return EXIT_FAILURE;
}

/**
 * @brief Comando para renomear um arquivo no sistema de arquivos EXT2.
 *
 * Este comando renomeia um arquivo especificado por seu caminho para um novo nome.
 *
 * @param argc Número de argumentos da linha de comando.
 * @param argv Lista de argumentos da linha de comando.
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o inode do diretório de trabalho atual.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_rename(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 3) // Verifica se o número de argumentos é válido
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    char *old_path = argv[1]; // Caminho do arquivo a ser renomeado
    char *new_name = argv[2]; // Novo nome para o arquivo

    if (strlen(new_name) > 255) // Verifica se o novo nome é muito longo
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    char *old_full_path = fs_join_path(fs, *cwd, old_path); // Resolve o caminho completo do arquivo antigo
    if (!old_full_path)                                     // Se não conseguir juntar o caminho, retorna erro
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t old_inode;                                     // Variável para armazenar o inode do arquivo antigo
    if (fs_path_resolve(fs, old_full_path, &old_inode) < 0) // Resolve o inode do arquivo antigo
    {
        free(old_full_path);
        print_error(ERROR_FILE_NOT_FOUND);
        return EXIT_FAILURE;
    }

    char *parent_path;                         // Descobre o diretório pai do arquivo
    char *dup_path = strdup(old_full_path);    // Duplica o caminho completo do arquivo antigo
    char *last_slash = strrchr(dup_path, '/'); // Encontra a última barra no caminho

    if (!last_slash)                         // Se não encontrar barra, significa que é o diretório atual
        parent_path = fs_get_path(fs, *cwd); // Obtém o caminho do diretório atual

    else if (last_slash == dup_path) // Se a barra for o primeiro caractere, significa que é a raiz
        parent_path = strdup("/");   // Define o caminho do diretório pai como raiz

    else // Se encontrar uma barra, separa o caminho do diretório pai
    {
        *last_slash = '\0';
        parent_path = strdup(dup_path);
    }

    uint32_t parent_inode_num;
    if (fs_path_resolve(fs, parent_path, &parent_inode_num) < 0) // Resolve o inode do diretório pai
    {
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    struct ext2_inode parent_inode;
    if (fs_read_inode(fs, parent_inode_num, &parent_inode) < 0) // Lê o inode do diretório pai
    {
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    int already_exists = name_exists(fs, &parent_inode, new_name); // Verifica se já existe uma entrada com o novo nome
    if (already_exists < 0)                                        // Se ocorrer um erro ao verificar a existência do nome
    {
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (already_exists == 1) // Se já existe uma entrada com o novo nome, retorna erro
    {
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        print_error(ERROR_FILE_OR_DIRECTORY_ALREADY_EXISTS);
        return EXIT_FAILURE;
    }

    int renamed = 0;
    for (int i = 0; i < 12; ++i) // Procura e renomeia a entrada no diretório pai
    {
        uint32_t block = parent_inode.i_block[i]; // Obtém o bloco do diretório pai
        if (!block)                               // Se o bloco for 0, significa que não há mais blocos para verificar
            continue;
        if (!rename_entry_block(fs, block, old_inode, new_name)) // Renomeia a entrada no bloco
        {
            renamed = 1;
            break;
        }
    }
    if (!renamed) // Se não conseguiu renomear a entrada em nenhum bloco
    {
        free(old_full_path);
        free(dup_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    free(old_full_path);
    free(dup_path);
    free(parent_path);
    return EXIT_SUCCESS;
}
