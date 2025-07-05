#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "commands.h"

/**
 * @brief Adiciona uma nova entrada de diretório.
 *
 * Esta função aloca um novo bloco se necessário e adiciona uma entrada
 * de diretório com o nome especificado, associando-o ao inode fornecido.
 *
 * @param fs         Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode  Ponteiro para o inode do diretório onde a entrada será adicionada.
 * @param dir_ino    Número do inode do diretório onde a entrada será adiciona.
 * @param new_ino    Número do inode do novo arquivo ou diretório a ser adicionado.
 * @param name       Nome da nova entrada de diretório.
 * @param file_type  Tipo de arquivo da nova entrada (por exemplo, regular, diretório, etc.).
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
static int dir_add_entry(ext2_fs_t *fs, struct ext2_inode *dir_inode, uint32_t dir_ino, uint32_t new_ino, char *name, uint8_t file_type)
{
    uint8_t buf[EXT2_BLOCK_SIZE];
    uint16_t tamanho_necessario = rec_len_needed((uint8_t)strlen(name));

    // Percorre os blocos diretos do diretório
    for (int i = 0; i < 12; ++i)
    {
        uint32_t bloco = dir_inode->i_block[i];

        // Se o bloco não existe, aloca um novo bloco para o diretório
        if (!bloco)
        {
            if (fs_alloc_block(fs, &bloco) < 0)
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
            dir_inode->i_block[i] = bloco;
            dir_inode->i_size += EXT2_BLOCK_SIZE;
            dir_inode->i_blocks += EXT2_BLOCK_SIZE / 512;

            // Cria a nova entrada ocupando todo o bloco
            memset(buf, 0, EXT2_BLOCK_SIZE);
            struct ext2_dir_entry *entrada = (struct ext2_dir_entry *)buf; // Inicia a entrada de diretório
            entrada->inode = new_ino;
            entrada->rec_len = EXT2_BLOCK_SIZE;
            entrada->name_len = (uint8_t)strlen(name);
            entrada->file_type = file_type;
            memcpy(entrada->name, name, entrada->name_len); // Copia o nome para a entrada

            if (fs_write_block(fs, bloco, buf) < 0) // Escreve o bloco no disco
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
            if (fs_write_inode(fs, dir_ino, dir_inode) < 0) // Atualiza o inode do diretório
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

        if (fs_read_block(fs, bloco, buf) < 0) // Lê o bloco existente
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }

        uint32_t pos = 0;
        // Percorre as entradas do bloco procurando espaço livre
        while (pos < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *entrada = (struct ext2_dir_entry *)(buf + pos); // Obtém a entrada de diretório atual
            if (entrada->rec_len == 0)
                break;

            uint16_t ideal = rec_len_needed(entrada->name_len);
            uint16_t espaco_livre = entrada->rec_len - ideal;

            // Se houver espaço suficiente após esta entrada, insere a nova entrada aqui
            if (espaco_livre >= tamanho_necessario)
            {
                entrada->rec_len = ideal;
                struct ext2_dir_entry *nova = (struct ext2_dir_entry *)(buf + pos + ideal); // Nova entrada de diretório
                nova->inode = new_ino;
                nova->rec_len = espaco_livre;
                nova->name_len = (uint8_t)strlen(name);
                nova->file_type = file_type;
                memcpy(nova->name, name, nova->name_len); // Copia o nome para a nova entrada

                if (fs_write_block(fs, bloco, buf) < 0) // Escreve o bloco no disco
                {
                    print_error(ERROR_UNKNOWN);
                    return EXIT_FAILURE;
                }
                if (fs_write_inode(fs, dir_ino, dir_inode) < 0) // Atualiza o inode do diretório
                {
                    print_error(ERROR_UNKNOWN);
                    return EXIT_FAILURE;
                }
                return EXIT_SUCCESS;
            }
            pos += entrada->rec_len; // Avança para a próxima entrada
        }
    }

    // Não há espaço disponível nos blocos diretos
    print_error(ERROR_UNKNOWN);
    return EXIT_FAILURE;
}

/**
 * @brief Cria um novo diretório.
 *
 * Esta função cria um novo diretório no sistema de arquivos EXT2.
 *
 * @param argc  Número de argumentos da linha de comando.
 * @param argv  Argumentos da linha de comando.
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd   Ponteiro para o inode do diretório atual.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_mkdir(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2) // Verifica se o número de argumentos é correto
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    char *novo_caminho = fs_join_path(fs, *cwd, argv[1]); // Cria o caminho absoluto do novo diretório
    if (!novo_caminho)                                    // Verifica se a alocação do caminho foi bem-sucedida
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t inode_existente;
    if (fs_path_resolve(fs, novo_caminho, &inode_existente) == 0) // Verifica se já existe um arquivo ou diretório com esse nome
    {
        free(novo_caminho);
        print_error(ERROR_DIRECTORY_ALREADY_EXISTS);
        return EXIT_FAILURE;
    }

    char *ultimo_slash = strrchr(novo_caminho, '/');                 // Separa o caminho do diretório pai e o nome do novo diretório
    char *nome_dir = ultimo_slash ? ultimo_slash + 1 : novo_caminho; // Nome do novo diretório
    char *caminho_pai;                                               // Variável para armazenar o caminho do diretório pai
    if (ultimo_slash)                                                // Se houver um último '/' no caminho
    {
        if (ultimo_slash == novo_caminho) // Se o caminho for apenas '/'
            caminho_pai = strdup("/");
        else // Se houver um caminho pai
        {
            *ultimo_slash = '\0';
            caminho_pai = strdup(novo_caminho);
            *ultimo_slash = '/';
        }
    }
    else // Se não houver um último '/', o diretório pai é o atual
    {
        caminho_pai = fs_get_path(fs, *cwd); // Obtém o caminho do diretório atual
    }
    if (!caminho_pai) // Verifica se a alocação do caminho pai foi bem-sucedida
    {
        free(novo_caminho);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t inode_pai;
    if (fs_path_resolve(fs, caminho_pai, &inode_pai) < 0) // Resolve o inode do diretório pai
    {
        free(novo_caminho);
        free(caminho_pai);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    struct ext2_inode inode_pai_struct;                      // Estrutura para armazenar o inode do diretório pai
    if (fs_read_inode(fs, inode_pai, &inode_pai_struct) < 0) // Lê o inode do diretório pai
    {
        free(novo_caminho);
        free(caminho_pai);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (!ext2_is_dir(&inode_pai_struct)) // Verifica se o inode do pai é um diretório
    {
        free(novo_caminho);
        free(caminho_pai);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t novo_inode;                                          // Variável para armazenar o número do novo inode
    if (fs_alloc_inode(fs, EXT2_S_IFDIR | 0755, &novo_inode) < 0) // Aloca um novo inode para o diretório
    {
        free(novo_caminho);
        free(caminho_pai);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t novo_bloco;                     // Aloca o primeiro bloco do novo diretório
    if (fs_alloc_block(fs, &novo_bloco) < 0) // Aloca um novo bloco para o diretório
    {
        free(novo_caminho);
        free(caminho_pai);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Prepara o inode do novo diretório
    struct ext2_inode novo_inode_struct = {0};
    novo_inode_struct.i_mode = EXT2_S_IFDIR | 0755;
    novo_inode_struct.i_uid = 0;
    novo_inode_struct.i_gid = 0;
    novo_inode_struct.i_size = EXT2_BLOCK_SIZE;
    novo_inode_struct.i_blocks = EXT2_BLOCK_SIZE / 512;
    novo_inode_struct.i_links_count = 2; // '.' e '..'
    time_t agora = time(NULL);
    novo_inode_struct.i_atime = novo_inode_struct.i_ctime = novo_inode_struct.i_mtime = (uint32_t)agora; // Define os tempos de acesso, criação e modificação
    novo_inode_struct.i_block[0] = novo_bloco;

    if (fs_write_inode(fs, novo_inode, &novo_inode_struct) < 0) // Escreve o inode do novo diretório no disco
    {
        free(novo_caminho);
        free(caminho_pai);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Cria as entradas '.' e '..' no novo diretório
    uint8_t buffer[EXT2_BLOCK_SIZE];
    memset(buffer, 0, EXT2_BLOCK_SIZE);
    struct ext2_dir_entry *ponto = (struct ext2_dir_entry *)buffer; // Posição da entrada '.'
    ponto->inode = novo_inode;
    ponto->name_len = 1;
    ponto->file_type = EXT2_FT_DIR;
    ponto->rec_len = rec_len_needed(1);
    ponto->name[0] = '.';

    struct ext2_dir_entry *ponto_ponto = (struct ext2_dir_entry *)(buffer + ponto->rec_len); // Posição da entrada '..'
    ponto_ponto->inode = inode_pai;
    ponto_ponto->name_len = 2;
    ponto_ponto->file_type = EXT2_FT_DIR;
    ponto_ponto->rec_len = EXT2_BLOCK_SIZE - ponto->rec_len;
    ponto_ponto->name[0] = '.';
    ponto_ponto->name[1] = '.';

    if (fs_write_block(fs, novo_bloco, buffer) < 0) // Escreve o bloco do novo diretório no disco
    {
        free(novo_caminho);
        free(caminho_pai);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (dir_add_entry(fs, &inode_pai_struct, inode_pai, novo_inode, nome_dir, EXT2_FT_DIR)) // Adiciona a entrada do novo diretório no diretório pai
    {
        free(novo_caminho);
        free(caminho_pai);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    inode_pai_struct.i_links_count++;                 // Atualiza o link count do diretório pai
    fs_write_inode(fs, inode_pai, &inode_pai_struct); // Escreve o inode do diretório pai no disco

    free(novo_caminho);
    free(caminho_pai);
    return EXIT_SUCCESS;
}
