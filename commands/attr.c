#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "commands.h"

/**
 * @brief Constrói uma string de permissões a partir do inode.
 *
 * Esta função converte o modo do inode em uma string de 11 caracteres representando
 * o tipo de arquivo e as permissões de leitura, escrita e execução para o usuário, grupo e outros.
 *
 * @param inode Ponteiro para o inode do arquivo ou diretório.
 * @param out   String de saída onde as permissões serão armazenadas.
 *
 * @return Nenhum.
 */
static void build_perm_string(struct ext2_inode *inode, char out[11])
{
    uint16_t mode = inode->i_mode;

    // Tipo de arquivo
    if ((mode & 0xF000) == EXT2_S_IFDIR)
        out[0] = 'd';
    else if ((mode & 0xF000) == EXT2_S_IFREG)
        out[0] = '-';
    else if ((mode & 0xF000) == EXT2_S_IFLNK)
        out[0] = 'l';
    else if ((mode & 0xF000) == EXT2_S_IFCHR)
        out[0] = 'c';
    else if ((mode & 0xF000) == EXT2_S_IFBLK)
        out[0] = 'b';
    else if ((mode & 0xF000) == EXT2_S_IFIFO)
        out[0] = 'p';
    else if ((mode & 0xF000) == EXT2_S_IFSOCK)
        out[0] = 's';
    else
        out[0] = '?';

    // Permissões: usuário, grupo, outros
    out[1] = (mode & EXT2_S_IRUSR) ? 'r' : '-';
    out[2] = (mode & EXT2_S_IWUSR) ? 'w' : '-';
    out[3] = (mode & EXT2_S_IXUSR) ? 'x' : '-';
    out[4] = (mode & EXT2_S_IRGRP) ? 'r' : '-';
    out[5] = (mode & EXT2_S_IWGRP) ? 'w' : '-';
    out[6] = (mode & EXT2_S_IXGRP) ? 'x' : '-';
    out[7] = (mode & EXT2_S_IROTH) ? 'r' : '-';
    out[8] = (mode & EXT2_S_IWOTH) ? 'w' : '-';
    out[9] = (mode & EXT2_S_IXOTH) ? 'x' : '-';
    out[10] = '\0';
}

/**
 * @brief Converte bytes em string humana (B, KiB, MiB)
 *
 * Esta função converte um valor em bytes para uma representação de string mais legível,
 * usando as unidades apropriadas (B, KiB, MiB).
 *
 * @param bytes  O valor em bytes a ser convertido.
 * @param out    O buffer de saída onde a string resultante será armazenada.
 * @param outsz  O tamanho do buffer de saída.
 *
 * @return Nenhum.
 */
static void human_size(uint32_t bytes, char *out, size_t outsz)
{
    if (bytes < 1024) // Se for menor que 1 KiB, exibe em bytes
    {
        snprintf(out, outsz, "%u B", bytes);
    }
    else if (bytes < 1024 * 1024) // Se for menor que 1 MiB, exibe em KiB
    {
        double k = bytes / 1024.0;
        snprintf(out, outsz, "%.1f KiB", k);
    }
    else if (bytes < 1024 * 1024 * 1024) // Se for menor que 1 GiB, exibe em MiB
    {
        double m = bytes / 1048576.0;
        snprintf(out, outsz, "%.1f MiB", m);
    }
}

/**
 * @brief Comando para exibir atributos de um arquivo ou diretório.
 *
 * Este comando recebe o nome de um arquivo ou diretório e exibe:
 * Permissões, UID, GID, tamanho e data de modificação.
 *
 * @param argc Número de argumentos passados para o comando.
 * @param argv Array de strings contendo os argumentos do comando.
 * @param fs Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd Ponteiro para o inode do diretório atual.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro.
 */
int cmd_attr(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd)
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: attr <arquivo|diretório>\n");
        errno = EINVAL;
        return -1;
    }

    char *full_path = fs_join_path(fs, *cwd, argv[1]); // Combina o diretório atual com o caminho fornecido
    if (!full_path)
        return -1;

    uint32_t inode_num;
    if (fs_path_resolve(fs, full_path, &inode_num) < 0) // Resolve o caminho para obter o inode correspondente
    {
        fprintf(stderr, "Erro: caminho '%s' não encontrado.\n", argv[1]);
        free(full_path);
        return -1;
    }

    struct ext2_inode inode;
    if (fs_read_inode(fs, inode_num, &inode) < 0) // Lê o inode do arquivo ou diretório
    {
        fprintf(stderr, "Erro ao ler inode.\n");
        free(full_path);
        return -1;
    }

    char perm_str[11];
    build_perm_string(&inode, perm_str); // Constrói a string de permissões a partir do inode

    char size_str[32];
    human_size(inode.i_size, size_str, sizeof(size_str)); // Converte o tamanho do arquivo em uma string legível

    char mod_date[20];
    time_t mod_time = inode.i_mtime;
    struct tm tm_info;
    localtime_r(&mod_time, &tm_info);
    strftime(mod_date, sizeof(mod_date), "%d/%m/%Y %H:%M", &tm_info); // Formata a data de modificação

    printf("%-11s %-6s %-6s %-12s %-17s\n", "Permissões", "UID", "GID", "Tamanho", "Modificado em");
    printf("%-10s %-6u %-6u %-12s %-17s\n",
           perm_str,
           inode.i_uid,
           inode.i_gid,
           size_str,
           mod_date);

    free(full_path);
    return 0;
}
