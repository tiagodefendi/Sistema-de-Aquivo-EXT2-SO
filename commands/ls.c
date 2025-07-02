#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
static int list_directory(ext2_fs_t *fs, struct ext2_inode *dir_inode)
{
    uint8_t buf[EXT2_BLOCK_SIZE];

    // Percorre os blocos diretos do inode do diretório
    for (int i = 0; i < 12; ++i)
    {
        uint32_t bloco = dir_inode->i_block[i];
        if (bloco == 0)
            continue;

        if (fs_read_block(fs, bloco, buf) < 0)
            return -1;

        uint32_t offset = 0;
        while (offset < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buf + offset);
            if (entry->rec_len == 0)
                break; // Bloco corrompido

            if (entry->inode != 0)
            {
                print_entry(entry);
            }
            offset += entry->rec_len;
        }
    }
    return 0;
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
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_ls(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc > 2)
    {
        fprintf(stderr, "Uso: ls [caminho]\n");
        errno = EINVAL;
        return -1;
    }

    uint32_t alvo_inode;
    char *caminho_completo = NULL;

    // Se nenhum argumento, lista o diretório atual
    if (argc == 1)
    {
        alvo_inode = *cwd;
    }
    else
    {
        caminho_completo = fs_join_path(fs, *cwd, argv[1]);
        if (!caminho_completo)
            return -1;
        if (fs_path_resolve(fs, caminho_completo, &alvo_inode) < 0)
        {
            free(caminho_completo);
            return -1;
        }
    }

    struct ext2_inode inode;
    if (fs_read_inode(fs, alvo_inode, &inode) < 0)
    {
        free(caminho_completo);
        return -1;
    }

    int resultado;
    if (ext2_is_dir(&inode))
    {
        resultado = list_directory(fs, &inode);
    }
    else
    {
        struct ext2_dir_entry entrada_fake = {
            .inode = alvo_inode,
            .rec_len = inode.i_blocks ? inode.i_blocks * 512 : 0,
            .name_len = (uint8_t)strlen(argv[argc == 1 ? 0 : 1]),
            .file_type = EXT2_FT_REG_FILE};
        strncpy((char *)entrada_fake.name, argc == 1 ? "(arquivo)" : argv[1], entrada_fake.name_len);
        print_entry(&entrada_fake);
        resultado = 0;
    }

    free(caminho_completo);
    return resultado;
}
