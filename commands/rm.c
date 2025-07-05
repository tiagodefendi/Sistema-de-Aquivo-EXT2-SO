#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "commands.h"

/**
 * @brief   Libera um bloco no sistema de arquivos EXT2.
 *
 * Esta função marca um bloco como livre, atualizando o bitmap de blocos e o descritor de grupo correspondente.
 *
 * @param fs  Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param blk Número do bloco a ser liberado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se o bloco já estiver livre).
 */
int fs_free_blocks(ext2_fs_t *fs, uint32_t blk)
{
    struct ext2_group_desc gd;
    uint8_t bitmap[EXT2_BLOCK_SIZE];

    /* Grupo ao qual o bloco pertence */
    uint32_t group = (blk - fs->sb.s_first_data_block) / fs->sb.s_blocks_per_group;

    if (fs_read_group_desc(fs, group, &gd) < 0)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (fs_read_block(fs, gd.bg_block_bitmap, bitmap) < 0)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t idx = (blk - fs->sb.s_first_data_block) % fs->sb.s_blocks_per_group;

    /* bloco já livre → inconsistência */
    if (!(bitmap[BIT_BYTE(idx)] & BIT_MASK(idx)))
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    bitmap[BIT_BYTE(idx)] &= ~BIT_MASK(idx); /* marca como livre */
    gd.bg_free_blocks_count++;
    fs->sb.s_free_blocks_count++;

    if (fs_write_block(fs, gd.bg_block_bitmap, bitmap) < 0)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }
    if (fs_write_group_desc(fs, group, &gd) < 0)
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    return fs_sync_super(fs);
}

/**
 * @brief   Libera uma cadeia de blocos indiretos (simples ou duplos).
 *
 * Esta função libera todos os blocos referenciados por um bloco indireto, recursivamente.
 * Se a profundidade for 1, libera blocos indiretos simples; se for 2, libera blocos indiretos duplos.
 *
 * @param fs     Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param blk    Bloco a ser liberado.
 * @param depth  Profundidade da cadeia (1 para indireto simples, 2 para indireto duplo).
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int free_indirect_chain(ext2_fs_t *fs, uint32_t blk, int depth)
{
    if (!blk)
        return EXIT_SUCCESS;

    uint32_t ptrs[PTRS_PER_BLOCK];        // Ponteiros para blocos
    if (fs_read_block(fs, blk, ptrs) < 0) // Lê o bloco indireto
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (depth == 1) // Se for indireto simples
    {
        for (size_t i = 0; i < PTRS_PER_BLOCK; ++i)     // Percorre os ponteiros
            if (ptrs[i] && fs_free_blocks(fs, ptrs[i])) // Libera blocos diretos
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
    }
    else // depth == 2
    {
        for (size_t i = 0; i < PTRS_PER_BLOCK; ++i)                     // Percorre os ponteiros
            if (ptrs[i] && free_indirect_chain(fs, ptrs[i], depth - 1)) // Libera blocos indiretos
            {
                print_error(ERROR_UNKNOWN);
                return EXIT_FAILURE;
            }
    }

    return fs_free_blocks(fs, blk); // Libera o bloco indireto em si
}

/**
 * @brief   Libera todos os blocos de dados associados a um inode.
 *
 * Esta função libera todos os blocos diretos, indiretos simples e indiretos duplos de um inode,
 * além de zerar os ponteiros e atualizar as estatísticas do inode.
 *
 * @param fs  Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param ino Ponteiro para o inode a ser liberado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int free_inode_block(ext2_fs_t *fs, struct ext2_inode *ino)
{
    // Zera os ponteiros e estatísticas do inode
    for (int i = 0; i < 12; ++i) // Libera diretos
    {
        if (ino->i_block[i] && fs_free_blocks(fs, ino->i_block[i])) // Libera diretos
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }
    }

    if (free_indirect_chain(fs, ino->i_block[12], 1)) // Libera indiretos simples
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (free_indirect_chain(fs, ino->i_block[13], 2)) // Libera indireto duplo
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (free_indirect_chain(fs, ino->i_block[14], 3) < 0) // Libera indireto triplo
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    // Zera os ponteiros de blocos
    memset(ino->i_block, 0, sizeof(ino->i_block));
    ino->i_blocks = 0;
    ino->i_size = 0;
    ino->i_dtime = time(NULL);

    return EXIT_SUCCESS;
}

/**
 * @brief   Remove uma entrada de diretório pelo número do inode.
 *
 * Esta função percorre os blocos diretos de um inode de diretório e remove a entrada correspondente ao inode alvo.
 * Se a entrada for encontrada, ela é zerada e o bloco é atualizado.
 *
 * @param fs         Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode  Ponteiro para o inode do diretório onde a entrada será removida.
 * @param target_ino Número do inode da entrada a ser removida.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se a entrada não for encontrada).
 */
int dir_remove_entry_rm(ext2_fs_t *fs, struct ext2_inode *dir_inode, uint32_t target_ino)
{
    uint8_t buf[EXT2_BLOCK_SIZE]; // Buffer para armazenar o bloco lido

    for (int i = 0; i < 12; ++i) // Percorre os blocos diretos do diretório
    {
        uint32_t bloco = dir_inode->i_block[i]; // Obtém o bloco atual
        if (!bloco)                             // Se o bloco não existe, continua para o próximo
            continue;

        if (fs_read_block(fs, bloco, buf) < 0) // Lê o bloco do diretório
        {
            print_error(ERROR_UNKNOWN);
            return EXIT_FAILURE;
        }

        uint32_t off = 0; // Posição atual no buffer
        struct ext2_dir_entry *prev = NULL;

        while (off < EXT2_BLOCK_SIZE) // Percorre o bloco até o final
        {
            struct ext2_dir_entry *e = (void *)(buf + off); // Obtém a entrada de diretório atual
            if (!e->rec_len)                                // Se a entrada não tiver comprimento, significa que não há mais entradas
                break;

            if (e->inode == target_ino)
            {
                if (prev) // Funde o espaço da entrada removida no rec_len da anterior
                    prev->rec_len += e->rec_len;
                else // se for a primeira entrada do bloco, marcaremos ela como livre estendendo-a até o fim do bloco
                {
                    e->inode = 0;
                    e->rec_len = EXT2_BLOCK_SIZE;
                }

                if (fs_write_block(fs, bloco, buf) < 0) // Atualiza o bloco com a entrada removida
                {
                    print_error(ERROR_UNKNOWN);
                    return EXIT_FAILURE;
                }
                return EXIT_SUCCESS;
            }
            prev = e;
            off += e->rec_len;
        }
    }
    print_error(ERROR_UNKNOWN);
    return EXIT_FAILURE;
}

/**
 * @brief   Comando 'rm' para remover um arquivo.
 *
 * Este comando aceita um argumento que especifica o arquivo a ser removido.
 * Se o arquivo for um diretório, exibe uma mensagem de erro e não o remove.
 *
 * @param argc  Número de argumentos passados para o comando.
 * @param argv  Array de strings contendo os argumentos.
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd   Ponteiro para o inode do diretório corrente.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_rm(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2) // Verifica se o número de argumentos é correto
    {
        print_error(ERROR_INVALID_SYNTAX);
        return EXIT_FAILURE;
    }

    char *full_path = fs_join_path(fs, *cwd, argv[1]); // Junta o caminho atual com o nome do arquivo
    if (!full_path)                                    // Se não conseguir juntar o caminho, exibe erro
    {
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    uint32_t file_ino;                                 // Variável para armazenar o inode do arquivo
    if (fs_path_resolve(fs, full_path, &file_ino) < 0) // Resolve o inode do arquivo
    {
        free(full_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    struct ext2_inode file_inode;                     // Estrutura para armazenar o inode do arquivo
    if (fs_read_inode(fs, file_ino, &file_inode) < 0) // Lê o inode do arquivo
    {
        free(full_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (ext2_is_dir(&file_inode)) // Verifica se é um diretório
    {
        free(full_path);
        print_error(ERROR_FILE_NOT_FOUND);
        return EXIT_FAILURE;
    }

    char *parent_path = strdup(full_path);   // Duplica o caminho completo
    char *slash = strrchr(parent_path, '/'); // Encontra a última barra no caminho
    if (slash && slash != parent_path)       // Se houver uma barra e não for o início do caminho
        *slash = '\0';
    else // Se não houver barra ou for o início do caminho
        strcpy(parent_path, "/");

    uint32_t parent_ino;
    if (fs_path_resolve(fs, parent_path, &parent_ino) < 0) // Resolve o inode do diretório pai
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    struct ext2_inode parent_inode;
    if (fs_read_inode(fs, parent_ino, &parent_inode) < 0) // Lê o inode do diretório pai
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (dir_remove_entry_rm(fs, &parent_inode, file_ino) < 0) // Remove a entrada do diretório pai
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (free_inode_block(fs, &file_inode)) // Libera os blocos do inode
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    if (fs_free_inode(fs, file_ino) < 0) // Libera o inode
    {
        free(full_path);
        free(parent_path);
        print_error(ERROR_UNKNOWN);
        return EXIT_FAILURE;
    }

    free(full_path);
    free(parent_path);
    return EXIT_SUCCESS;
}
