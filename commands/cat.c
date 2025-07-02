#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "commands.h"

/**
 * @brief   Despeja o conteúdo de um arquivo regular para a saída padrão.
 *
 * Esta função lê os blocos de dados do inode do arquivo e escreve seu conteúdo na saída padrão.
 * Suporta blocos diretos e um bloco indireto.
 *
 * @param fs     Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param inode  Ponteiro para o inode do arquivo a ser lido.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se ocorrer falha na leitura).
 */
static int dump_file(ext2_fs_t *fs, struct ext2_inode *inode)
{
    uint32_t file_size = inode->i_size;
    uint32_t bytes_left = file_size;
    uint8_t block_buf[EXT2_BLOCK_SIZE];

    // Ler blocos diretos
    for (int i = 0; i < 12 && bytes_left > 0; ++i)
    {
        uint32_t block_num = inode->i_block[i];
        if (block_num == 0)
            break;

        if (fs_read_block(fs, block_num, block_buf) < 0)
            return -1;

        uint32_t bytes_to_write = bytes_left < EXT2_BLOCK_SIZE ? bytes_left : EXT2_BLOCK_SIZE;
        if (fwrite(block_buf, 1, bytes_to_write, stdout) != bytes_to_write)
            return -1;

        bytes_left -= bytes_to_write;
    }

    // Ler bloco indireto simples (se necessário)
    if (bytes_left > 0)
    {
        uint32_t indirect_block = inode->i_block[12];
        if (indirect_block == 0)
        {
            errno = EIO;
            return -1;
        }

        if (fs_read_block(fs, indirect_block, block_buf) < 0)
            return -1;

        uint32_t *indirect_entries = (uint32_t *)block_buf;
        uint32_t entries_per_block = EXT2_BLOCK_SIZE / sizeof(uint32_t);

        for (uint32_t i = 0; i < entries_per_block && bytes_left > 0; ++i)
        {
            uint32_t block_num = indirect_entries[i];
            if (block_num == 0)
                break;

            if (fs_read_block(fs, block_num, block_buf) < 0)
                return -1;

            uint32_t bytes_to_write = bytes_left < EXT2_BLOCK_SIZE ? bytes_left : EXT2_BLOCK_SIZE;
            if (fwrite(block_buf, 1, bytes_to_write, stdout) != bytes_to_write)
                return -1;

            bytes_left -= bytes_to_write;
        }
    }

    return 0;
}

/**
 * @brief   Comando para exibir o conteúdo de um arquivo regular.
 *
 * Este comando recebe o nome de um arquivo e exibe seu conteúdo na saída padrão.
 * Se o arquivo não for regular, retorna um erro.
 *
 * @param argc  Número de argumentos passados para o comando.
 * @param argv  Array de strings contendo os argumentos do comando.
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd   Ponteiro para o inode do diretório atual.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_cat(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: cat <arquivo>\n");
        errno = EINVAL;
        return -1;
    }

    // Monta o caminho absoluto do arquivo
    char *caminho_completo = fs_join_path(fs, *cwd, argv[1]);
    if (!caminho_completo)
        return -1;

    // Resolve o caminho para obter o número do inode
    uint32_t inode_num;
    if (fs_path_resolve(fs, caminho_completo, &inode_num) < 0)
    {
        free(caminho_completo);
        return -1;
    }

    // Lê o inode do arquivo
    struct ext2_inode inode;
    if (fs_read_inode(fs, inode_num, &inode) < 0)
    {
        free(caminho_completo);
        return -1;
    }

    // Verifica se é um arquivo regular
    if (!ext2_is_reg(&inode))
    {
        fprintf(stderr, "%s: não é um arquivo regular\n", argv[1]);
        free(caminho_completo);
        errno = EISDIR;
        return -1;
    }

    // Exibe o conteúdo do arquivo
    int resultado = dump_file(fs, &inode);
    putchar('\n');
    free(caminho_completo);
    return resultado;
}
