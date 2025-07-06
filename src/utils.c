#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

/**
 * @brief   Abre uma imagem de sistema de arquivos EXT2.
 *
 * Esta função abre a imagem do sistema de arquivos especificada por 'img_path',
 * lê o superbloco e inicializa a estrutura ext2_fs_t.
 *
 * @param img_path Caminho para a imagem do sistema de arquivos EXT2.
 * @return Ponteiro para a estrutura ext2_fs_t ou NULL em caso de erro.
 */
ext2_fs_t *fs_open(char *img_path)
{
    // Abre o arquivo da imagem para leitura e escrita binária
    FILE *fp = fopen(img_path, "rb+");
    if (!fp)
        return NULL;

    ext2_fs_t *fs = calloc(1, sizeof(ext2_fs_t));
    if (!fs)
    {
        fclose(fp);
        return NULL;
    }

    // Lê o superbloco a partir do offset 1024 bytes
    if (fseek(fp, EXT2_SUPER_OFFSET, SEEK_SET) != 0)
    {
        free(fs);
        fclose(fp);
        return NULL;
    }
    size_t sb_read = fread(&fs->sb, 1, sizeof(fs->sb), fp);
    if (sb_read != sizeof(fs->sb))
    {
        free(fs);
        fclose(fp);
        return NULL;
    }

    // Verifica assinatura mágica
    if (fs->sb.s_magic != EXT2_SUPER_MAGIC)
    {
        free(fs);
        fclose(fp);
        return NULL;
    }

    // Calcula número de grupos de blocos
    fs->groups_count = (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) / fs->sb.s_blocks_per_group;

    // Salva descritor de arquivo
    fs->fd = fileno(fp);

    return fs;
}

/**
 * @brief   Fecha uma imagem de sistema de arquivos EXT2.
 *
 * Esta função sincroniza o superbloco, fecha o descritor de arquivo
 * e libera a memória alocada para a estrutura ext2_fs_t.
 *
 * @param   fs  Ponteiro para a estrutura ext2_fs_t a ser fechada.
 * @return  Nenhum valor é retornado.
 */
void fs_close(ext2_fs_t *fs)
{
    if (!fs)
        return;
    fs_sync_super(fs);
    close(fs->fd);
    free(fs);
}

/**
 * @brief   Calcula o deslocamento de um bloco no sistema de arquivos.
 *
 * @param   fs      Ponteiro para a estrutura ext2_fs_t.
 * @param   block   Número do bloco.
 *
 * @return  O deslocamento do bloco em bytes.
 */
off_t fs_block_offset(ext2_fs_t *fs, uint32_t block)
{
    (void)fs;
    return (off_t)block * EXT2_BLOCK_SIZE; // Calcula o deslocamento do bloco
}

/**
 * @brief   Lê um bloco de dados do sistema de arquivos EXT2.
 *
 * Esta função lê um bloco de dados da imagem do sistema de arquivos e armazena os dados no buffer fornecido.
 *
 * @param   fs      Ponteiro para a estrutura ext2_fs_t.
 * @param   block   Número do bloco a ser lido.
 * @param   buf     Buffer onde os dados lidos serão armazenados.
 *
 * @return  Retorna 0 em caso de sucesso ou -1 em caso de erro.
 */
int fs_read_block(ext2_fs_t *fs, uint32_t block, void *buf)
{
    ssize_t bytes_read = pread(fs->fd, buf, EXT2_BLOCK_SIZE, fs_block_offset(fs, block)); // Lê o bloco
    if (bytes_read == EXT2_BLOCK_SIZE)
        return 0;
    return -1;
}

/**
 * @brief   Escreve um bloco de dados no sistema de arquivos EXT2.
 *
 * Esta função escreve um bloco de dados na imagem do sistema de arquivos.
 *
 * @param   fs      Ponteiro para a estrutura ext2_fs_t.
 * @param   block   Número do bloco.
 * @param   buf     Buffer contendo os dados a serem escritos.
 *
 * @return  Retorna 0 em caso de sucesso ou -1 em caso de erro.
 */
int fs_write_block(ext2_fs_t *fs, uint32_t block, void *buf)
{
    ssize_t bytes_written = pwrite(fs->fd, buf, EXT2_BLOCK_SIZE, fs_block_offset(fs, block)); // Escreve o bloco
    if (bytes_written == EXT2_BLOCK_SIZE)
        return 0;
    return -1;
}

/**
 * @brief Calcula o deslocamento (offset) do descritor de grupo no sistema de arquivos EXT2.
 *
 * Esta função retorna o deslocamento em bytes onde o descritor de grupo de número 'group'
 * está localizado dentro do sistema de arquivos EXT2. O cálculo considera o offset do superbloco,
 * o tamanho do bloco e o índice do grupo multiplicado pelo tamanho da estrutura de descritor de grupo.
 *
 * @param group Número do grupo cujo offset do descritor será calculado.
 *
 * @return O deslocamento (offset) em bytes do descritor de grupo especificado.
 */
off_t gd_offset(uint32_t group)
{
    off_t group_desc_table_offset = EXT2_SUPER_OFFSET + EXT2_BLOCK_SIZE;     // Offset do início da tabela de descritores de grupo
    return group_desc_table_offset + group * sizeof(struct ext2_group_desc); // Calcula o offset do descritor de grupo
}

/**
 * @brief Lê o descritor de grupo do sistema de arquivos EXT2.
 *
 * Esta função lê o descritor de grupo especificado pelo índice 'group' do sistema de arquivos EXT2
 * apontado por 'fs' e armazena o resultado na estrutura 'gd'. Utiliza a função pread para ler os
 * bytes correspondentes diretamente do arquivo de dispositivo ou imagem do sistema de arquivos.
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param group Índice do grupo cujo descritor deve ser lido.
 * @param gd    Ponteiro para a estrutura onde o descritor de grupo lido será armazenado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro na leitura.
 */
int fs_read_group_desc(ext2_fs_t *fs, uint32_t group, struct ext2_group_desc *gd)
{
    ssize_t bytes_read = pread(fs->fd, gd, sizeof(*gd), gd_offset(group)); // Lê o descritor de grupo
    if (bytes_read == (ssize_t)sizeof(*gd))
        return 0;
    return -1;
}

/**
 * @brief   Escreve o descritor de grupo no disco para um grupo específico do sistema de arquivos ext2.
 *
 * Esta função grava a estrutura de descritor de grupo fornecida (`gd`) no local apropriado do disco,
 * correspondente ao grupo identificado por `group`, usando o descritor de arquivo do sistema de arquivos (`fs->fd`).
 * Utiliza a função `pwrite` para garantir que a escrita ocorra no deslocamento correto, retornando 0 em caso de sucesso
 * ou -1 em caso de falha.
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos ext2.
 * @param group Índice do grupo cujo descritor será escrito.
 * @param gd    Ponteiro para a estrutura do descritor de grupo a ser gravada.
 *
 * @return Retorna 0 em caso de sucesso, -1 em caso de erro na escrita.
 */
int fs_write_group_desc(ext2_fs_t *fs, uint32_t group, struct ext2_group_desc *gd)
{
    ssize_t bytes_written = pwrite(fs->fd, gd, sizeof(*gd), gd_offset(group)); // Escreve o descritor de grupo
    if (bytes_written == (ssize_t)sizeof(*gd))
        return 0;
    return -1;
}

/**
 * @brief   Localiza o inode no sistema de arquivos EXT2.
 *
 * Esta função calcula o grupo e o deslocamento do inode especificado por 'ino'.
 * O resultado é armazenado em 'gd_out' e 'off'.
 *
 * @param fs       Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param ino      Número do inode a ser localizado.
 * @param gd_out   Ponteiro para a estrutura onde o descritor de grupo será armazenado.
 * @param off      Ponteiro onde o deslocamento do inode será armazenado.
 *
 * @return Retorna 0 em caso de sucesso, -1 em caso de erro (por exemplo, se 'ino' for inválido).
 */
int inode_loc(ext2_fs_t *fs, uint32_t ino, struct ext2_group_desc *gd_out, off_t *off)
{
    if (ino == 0)
    {
        return -1;
    }

    // Inodes começam em 1
    uint32_t inode_index = ino - 1;

    // Descobre o grupo e o índice dentro do grupo
    uint32_t group = inode_index / fs->sb.s_inodes_per_group;
    uint32_t index_in_group = inode_index % fs->sb.s_inodes_per_group;

    // Lê o descritor de grupo correspondente
    if (fs_read_group_desc(fs, group, gd_out) < 0)
        return -1;

    // Calcula o deslocamento do inode na tabela de inodes do grupo
    off_t inode_table_offset = fs_block_offset(fs, gd_out->bg_inode_table);
    *off = inode_table_offset + (off_t)index_in_group * fs->sb.s_inode_size;

    return 0;
}

/**
 * @brief   Lê um inode do sistema de arquivos EXT2.
 *
 * Esta função lê o inode especificado por 'ino' e armazena os dados na estrutura 'inode'.
 * Utiliza a função pread para ler os bytes diretamente do arquivo de dispositivo ou imagem.
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param ino   Número do inode a ser lido.
 * @param inode Ponteiro para a estrutura onde o inode lido será armazenado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro na leitura.
 */
int fs_read_inode(ext2_fs_t *fs, uint32_t ino, struct ext2_inode *inode)
{
    struct ext2_group_desc group_desc;
    off_t inode_offset;

    // Localiza o descritor de grupo e o offset do inode
    if (inode_loc(fs, ino, &group_desc, &inode_offset) < 0)
        return -1;

    // Lê o inode do disco para a estrutura fornecida
    ssize_t bytes_read = pread(fs->fd, inode, sizeof(*inode), inode_offset);
    if (bytes_read != (ssize_t)sizeof(*inode))
        return -1;

    return 0;
}

/**
 * @brief   Escreve um inode no sistema de arquivos EXT2.
 *
 * Esta função escreve os dados do inode especificado por 'ino' na imagem do sistema de arquivos.
 * Utiliza a função pwrite para escrever os bytes diretamente no arquivo de dispositivo ou imagem.
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param ino   Número do inode a ser escrito.
 * @param inode Ponteiro para a estrutura contendo os dados do inode a serem escritos.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro na escrita.
 */
int fs_write_inode(ext2_fs_t *fs, uint32_t ino, struct ext2_inode *inode)
{
    struct ext2_group_desc group_desc;
    off_t inode_offset;

    // Localiza o descritor de grupo e o offset do inode
    if (inode_loc(fs, ino, &group_desc, &inode_offset) < 0)
        return -1;

    // Escreve o inode no disco a partir da estrutura fornecida
    ssize_t bytes_written = pwrite(fs->fd, inode, sizeof(*inode), inode_offset);
    if (bytes_written != (ssize_t)sizeof(*inode))
        return -1;
    return 0;
}

/**
 * @brief   Aloca um novo inode no sistema de arquivos EXT2.
 *
 * Esta função procura por um inode livre no sistema de arquivos e o aloca,
 * retornando o número do inode alocado em 'out_ino'.
 *
 * @param fs       Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param mode     Modo do inode a ser alocado (por exemplo, diretório ou arquivo regular).
 * @param out_ino  Ponteiro onde o número do inode alocado será armazenado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se não houver inodes livres).
 */
int fs_alloc_inode(ext2_fs_t *fs, uint16_t mode, uint32_t *out_ino)
{
    uint8_t bitmap[EXT2_BLOCK_SIZE];

    // Percorre todos os grupos de blocos
    for (uint32_t group = 0; group < fs->groups_count; ++group)
    {
        struct ext2_group_desc gd;
        if (fs_read_group_desc(fs, group, &gd) < 0)
            return -1;

        // Se não há inodes livres neste grupo, pula para o próximo
        if (gd.bg_free_inodes_count == 0)
            continue;

        // Lê o bitmap de inodes do grupo
        if (fs_read_block(fs, gd.bg_inode_bitmap, bitmap) < 0)
            return -1;

        // Procura por um inode livre no bitmap
        for (uint32_t idx = 0; idx < fs->sb.s_inodes_per_group; ++idx)
        {
            if (!(bitmap[BIT_BYTE(idx)] & BIT_MASK(idx)))
            {
                // Marca o inode como usado
                bitmap[BIT_BYTE(idx)] |= BIT_MASK(idx);
                if (fs_write_block(fs, gd.bg_inode_bitmap, bitmap) < 0)
                    return -1;

                gd.bg_free_inodes_count--;
                if ((mode & EXT2_S_IFDIR) == EXT2_S_IFDIR)
                    gd.bg_used_dirs_count++;
                if (fs_write_group_desc(fs, group, &gd) < 0)
                    return -1;

                fs->sb.s_free_inodes_count--;
                fs_sync_super(fs);

                // Calcula o número absoluto do inode (começa em 1)
                *out_ino = group * fs->sb.s_inodes_per_group + idx + 1;
                return 0;
            }
        }
    }
    return -1;
}

/**
 * @brief   Libera um inode no sistema de arquivos EXT2.
 *
 * Esta função marca um inode como livre, atualizando o bitmap de inodes e o descritor de grupo correspondente.
 *
 * @param fs  Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param ino Número do inode a ser liberado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se o inode já estiver livre).
 */
int fs_free_inode(ext2_fs_t *fs, uint32_t ino)
{
    struct ext2_group_desc gd;
    off_t inode_offset;

    // Localiza o grupo e o offset do inode
    if (inode_loc(fs, ino, &gd, &inode_offset) < 0)
        return -1;

    // Lê o bitmap de inodes do grupo
    uint8_t bitmap[EXT2_BLOCK_SIZE];
    if (fs_read_block(fs, gd.bg_inode_bitmap, bitmap) < 0)
        return -1;

    // Calcula o índice do inode dentro do grupo
    uint32_t index_in_group = (ino - 1) % fs->sb.s_inodes_per_group;

    // Verifica se o inode já está livre
    if (!(bitmap[BIT_BYTE(index_in_group)] & BIT_MASK(index_in_group)))
    {
        return -1;
    }

    // Marca o inode como livre no bitmap
    bitmap[BIT_BYTE(index_in_group)] &= ~BIT_MASK(index_in_group);
    if (fs_write_block(fs, gd.bg_inode_bitmap, bitmap) < 0)
        return -1;

    // Atualiza contadores de inodes livres
    gd.bg_free_inodes_count++;
    fs->sb.s_free_inodes_count++;

    // Atualiza descritor de grupo e superbloco
    uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
    if (fs_write_group_desc(fs, group, &gd) < 0)
        return -1;

    return fs_sync_super(fs); // Sincroniza o superbloco
}

/**
 * @brief   Imprime as informações de uma entrada de diretório.
 *
 * Esta função exibe o nome, inode, comprimento do registro, comprimento do nome
 * e tipo de arquivo de uma entrada de diretório.
 *
 * @param e  Ponteiro para a entrada de diretório a ser impressa.
 */
void print_entry(struct ext2_dir_entry *e)
{
    char name[256];
    size_t n = e->name_len < 255 ? e->name_len : 255;
    memcpy(name, e->name, n);
    name[n] = '\0';

    if (e->file_type == 1)
        printf("\033[32m%s\033[0m\n", name);
    else if (e->file_type == 2)
        printf("\033[34m%s\033[0m\n", name);

    printf("inode: %u\n", e->inode);
    printf("record length: %u\n", e->rec_len);
    printf("name length: %u\n", e->name_len);
    printf("file type: %u\n", e->file_type);
    printf("\n");
}

/**
 * @brief   Aloca um bloco livre no sistema de arquivos EXT2.
 *
 * Esta função procura por um bloco livre no sistema de arquivos e o aloca,
 * retornando o número do bloco alocado em 'out_block'.
 *
 * @param fs         Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param out_block  Ponteiro onde o número do bloco alocado será armazenado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se não houver blocos livres).
 */
int fs_alloc_block(ext2_fs_t *fs, uint32_t *out_block)
{
    uint8_t bitmap[EXT2_BLOCK_SIZE];

    // Percorre todos os grupos de blocos
    for (uint32_t group = 0; group < fs->groups_count; ++group)
    {
        struct ext2_group_desc gd;
        if (fs_read_group_desc(fs, group, &gd) < 0)
            return -1;

        // Se não há blocos livres neste grupo, pula para o próximo
        if (gd.bg_free_blocks_count == 0)
            continue;

        // Lê o bitmap de blocos do grupo
        if (fs_read_block(fs, gd.bg_block_bitmap, bitmap) < 0)
            return -1;

        // Procura por um bloco livre no bitmap
        for (uint32_t idx = 0; idx < fs->sb.s_blocks_per_group; ++idx)
        {
            if (!(bitmap[BIT_BYTE(idx)] & BIT_MASK(idx)))
            {
                // Marca o bloco como usado
                bitmap[BIT_BYTE(idx)] |= BIT_MASK(idx);
                if (fs_write_block(fs, gd.bg_block_bitmap, bitmap) < 0)
                    return -1;

                gd.bg_free_blocks_count--;
                if (fs_write_group_desc(fs, group, &gd) < 0)
                    return -1;

                fs->sb.s_free_blocks_count--;
                fs_sync_super(fs);

                // Calcula o número absoluto do bloco
                *out_block = fs->sb.s_first_data_block + group * fs->sb.s_blocks_per_group + idx;
                return 0;
            }
        }
    }
    return -1;
}

/**
 * @brief   Libera um bloco no sistema de arquivos EXT2.
 *
 * Esta função marca um bloco como livre, atualizando o bitmap de blocos e o descritor de grupo correspondente.
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param block Número do bloco a ser liberado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se o bloco já estiver livre).
 */
int fs_free_block(ext2_fs_t *fs, uint32_t block)
{
    if (block < fs->sb.s_first_data_block)
    {
        return -1;
    }
    uint32_t rel = block - fs->sb.s_first_data_block;
    uint32_t group = rel / fs->sb.s_blocks_per_group;
    uint32_t idx = rel % fs->sb.s_blocks_per_group;

    struct ext2_group_desc gd;
    if (fs_read_group_desc(fs, group, &gd) < 0)
    {
        return -1;
    }

    uint8_t bitmap[EXT2_BLOCK_SIZE];
    if (fs_read_block(fs, gd.bg_block_bitmap, bitmap) < 0)
    {
        return -1;
    }

    int bit = bitmap[BIT_BYTE(idx)] & BIT_MASK(idx);
              bitmap[BIT_BYTE(idx)], BIT_MASK(idx), !!bit);

    if (!bit)
    {
        return -1;
    }

    /* marca livre */
    bitmap[BIT_BYTE(idx)] &= ~BIT_MASK(idx);
    gd.bg_free_blocks_count++;
    fs->sb.s_free_blocks_count++;
    if (fs_write_block(fs, gd.bg_block_bitmap, bitmap) < 0)
    {
        return -1;
    }
    if (fs_write_group_desc(fs, group, &gd) < 0)
    {
        return -1;
    }

    if (fs_sync_super(fs) < 0)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief Libera todos os blocos de dados associados a um inode (diretos e indiretos simples).
 *
 * @param fs    Ponteiro para a estrutura do sistema de arquivos.
 * @param inode Ponteiro para o inode cujos blocos serão liberados.
 *
 * @return 0 em caso de sucesso, -1 em caso de erro.
 */
int free_inode_blocks(ext2_fs_t *fs, struct ext2_inode *inode)
{
    // Diretos
    for (int i = 0; i < 12; ++i)
    {
        uint32_t blk = inode->i_block[i];
        if (blk)
        {
            if (fs_free_block(fs, blk) < 0)
            {
                return -1;
            }
        }
    }
    // Indiretos simples, duplo e triplo
    uint32_t ib;
    ib = inode->i_block[12];
    ib = inode->i_block[13];
    ib = inode->i_block[14];

    // Use existing free_indirect_chain if available
    extern int free_indirect_chain(ext2_fs_t *, uint32_t, int);
    if (inode->i_block[12] && free_indirect_chain(fs, inode->i_block[12], 1) < 0)
        return -1;
    if (inode->i_block[13] && free_indirect_chain(fs, inode->i_block[13], 2) < 0)
        return -1;
    if (inode->i_block[14] && free_indirect_chain(fs, inode->i_block[14], 3) < 0)
        return -1;

    return 0;
}

/**
 * @brief   Verifica se um nome existe em um diretório.
 *
 * Esta função verifica se um nome específico já existe em um diretório,
 * retornando 1 se existir e 0 caso contrário.
 *
 * @param fs         Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode  Ponteiro para o inode do diretório onde o nome será verificado.
 * @param name       Nome a ser verificado no diretório.
 *
 * @return Retorna 1 se o nome existir, 0 caso contrário, ou -1 em caso de erro.
 */
int fs_iterate_dir(ext2_fs_t *fs, struct ext2_inode *dir_inode, dir_iter_cb cb, void *user)
{
    if (!ext2_is_dir(dir_inode))
    {
        return -1;
    }

    uint8_t block_buf[EXT2_BLOCK_SIZE];

    // Percorre apenas os blocos diretos do diretório
    for (int i = 0; i < 12; ++i)
    {
        uint32_t block_num = dir_inode->i_block[i];
        if (!block_num)
            continue; // Pula blocos não alocados

        if (fs_read_block(fs, block_num, block_buf) < 0)
            return -1;

        uint32_t offset = 0;
        while (offset < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(block_buf + offset);
            if (entry->rec_len == 0)
                break; // Entrada corrompida, para leitura

            if (entry->inode && cb)
            {
                int stop = cb(entry, user);
                if (stop)
                    return stop; // Callback pediu para parar
            }
            offset += entry->rec_len;
        }
    }
    return 0;
}

/**
 * @brief   Contexto para busca de nome em diretório.
 *
 * Esta estrutura é usada para armazenar o nome a ser buscado e o inode encontrado durante a iteração.
 */
struct dir_find_ctx
{
    char *name;   // Nome a ser buscado
    uint32_t ino; // Inode encontrado (0 se não encontrado)
};

/**
 * @brief   Callback para encontrar um nome específico em um diretório.
 *
 * Esta função é chamada para cada entrada de diretório durante a iteração,
 * verificando se o nome da entrada corresponde ao nome buscado.
 *
 * @param e  Ponteiro para a entrada de diretório atual.
 * @param u  Contexto de busca contendo o nome e o inode encontrado.
 *
 * @return Retorna 1 se o nome for encontrado, 0 caso contrário.
 */
static int find_cb(struct ext2_dir_entry *entry, void *user_ctx)
{
    struct dir_find_ctx *ctx = user_ctx; // Contexto de busca
    size_t name_len = strlen(ctx->name); // Comprimento do nome buscado

    if (entry->name_len == name_len && strncmp(entry->name, ctx->name, name_len) == 0) // Verifica se o nome corresponde
    {
        ctx->ino = entry->inode;
        return 1;
    }
    return 0;
}

/**
 * @brief   Encontra um inode por nome em um diretório.
 *
 * Esta função procura por um nome específico dentro de um diretório e retorna o inode correspondente.
 * Se o nome não for encontrado, define errno como ENOENT.
 *
 * @param fs         Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode  Ponteiro para o inode do diretório onde a busca será realizada.
 * @param name       Nome a ser buscado no diretório.
 * @param out_ino    Ponteiro onde o inode encontrado será armazenado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se o nome não for encontrado).
 */
int fs_find_in_dir(ext2_fs_t *fs, struct ext2_inode *dir_inode, char *name, uint32_t *out_ino)
{
    struct dir_find_ctx ctx = {.name = name, .ino = 0};   // Inicializa o contexto de busca
    if (fs_iterate_dir(fs, dir_inode, find_cb, &ctx) < 0) // Itera sobre o diretório
        return -1;
    if (ctx.ino == 0)
    {
        return -1;
    }
    *out_ino = ctx.ino; // Armazena o inode encontrado
    return 0;
}

/**
 * @brief   Calcula o tamanho necessário para uma entrada de diretório.
 *
 * Esta função calcula o tamanho necessário para armazenar uma entrada de diretório
 * com base no comprimento do nome fornecido. O tamanho é alinhado a 4 bytes.
 *
 * @param name_len Comprimento do nome da entrada.
 * @return Tamanho necessário em bytes.
 */
uint16_t rec_len_needed(uint8_t name_len)
{
    return (uint16_t)((8 + name_len + 3) & ~3);
}

/**
 * @brief   Resolve um caminho para obter o inode correspondente.
 *
 * Esta função percorre o caminho fornecido, resolvendo cada parte do caminho
 * e retornando o inode correspondente ao final do caminho.
 *
 * @param fs   Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param path Caminho a ser resolvido.
 * @param ino  Ponteiro onde o inode encontrado será armazenado.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro (por exemplo, se o caminho for inválido).
 */
int fs_path_resolve(ext2_fs_t *fs, char *path, uint32_t *ino)
{
    if (!path || !*path)
    {
        return -1;
    }

    uint32_t current_ino = EXT2_ROOT_INO;

    // Se o caminho é apenas "/", retorna a raiz
    if (strcmp(path, "/") == 0)
    {
        *ino = current_ino;
        return 0;
    }

    // Cria uma cópia do caminho para tokenização
    char *path_copy = strdup(path);
    if (!path_copy)
        return -1;

    char *saveptr = NULL;                             // Variável para armazenar o estado da tokenização
    char *token = strtok_r(path_copy, "/", &saveptr); // Tokeniza o caminho usando '/' como delimitador

    while (token)
    {
        struct ext2_inode dir_inode;
        if (fs_read_inode(fs, current_ino, &dir_inode) < 0) // Lê o inode do diretório atual
        {
            free(path_copy);
            return -1;
        }

        if (!ext2_is_dir(&dir_inode)) // Verifica se o inode atual é um diretório
        {
            free(path_copy);
            return -1;
        }

        if (fs_find_in_dir(fs, &dir_inode, token, &current_ino) < 0) // Procura o próximo componente no diretório atual
        {
            free(path_copy);
            return -1;
        }

        token = strtok_r(NULL, "/", &saveptr); // Continua tokenizando o caminho
    }

    free(path_copy);
    *ino = current_ino; // Armazena o inode encontrado
    return 0;
}

/**
 * @brief   Contexto para busca de nome de filho em diretório.
 *
 * Esta estrutura é usada para armazenar o inode e o nome do filho encontrado durante a iteração.
 */
struct child_ctx
{
    uint32_t ino;   // Inode do filho a ser buscado
    char name[256]; // Nome do filho encontrado
};

/**
 * @brief   Callback para encontrar um filho específico em um diretório.
 *
 * Esta função é chamada para cada entrada de diretório durante a iteração,
 * verificando se o inode da entrada corresponde ao inode buscado.
 *
 * @param entry     Ponteiro para a entrada de diretório atual.
 * @param user_ctx  Contexto de busca contendo o inode do filho e o nome encontrado.
 *
 * @return Retorna 1 se o filho for encontrado, 0 caso contrário.
 */
static int child_cb(struct ext2_dir_entry *entry, void *user_ctx)
{
    struct child_ctx *ctx = user_ctx; // Contexto de busca
    if (entry->inode == ctx->ino)     // Verifica se o inode da entrada corresponde ao inode buscado
    {
        size_t len = entry->name_len;
        if (len >= sizeof(ctx->name)) // Verifica se o nome é muito longo
            len = sizeof(ctx->name) - 1;
        memcpy(ctx->name, entry->name, len); // Copia o nome encontrado
        ctx->name[len] = '\0';               // Garante que o nome seja null-terminated
        return 1;
    }
    return 0;
}

/**
 * @brief   Obtém o caminho completo de um inode no sistema de arquivos EXT2.
 *
 * Esta função constrói o caminho completo a partir do inode fornecido, subindo pela hierarquia de diretórios
 * até chegar à raiz. O caminho é retornado como uma string alocada dinamicamente.
 *
 * @param fs       Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_ino  Número do inode do diretório cujo caminho será obtido.
 *
 * @return Retorna o caminho completo como uma string alocada dinamicamente, ou NULL em caso de erro.
 */
char *fs_get_path(ext2_fs_t *fs, uint32_t ino)
{
    if (ino == EXT2_ROOT_INO)
        return strdup("/");

    char *components[64];
    int count = 0;
    uint32_t current_ino = ino;

    while (current_ino != EXT2_ROOT_INO && count < 64) // Limita o número de componentes a 64
    {
        struct ext2_inode inode;
        if (fs_read_inode(fs, current_ino, &inode) < 0) // Lê o inode atual
            return NULL;

        uint32_t parent_ino;
        if (fs_find_in_dir(fs, &inode, "..", &parent_ino) < 0) // Procura o inode do pai
            return NULL;

        struct ext2_inode parent_inode;
        if (fs_read_inode(fs, parent_ino, &parent_inode) < 0) // Lê o inode do pai
            return NULL;

        struct child_ctx ctx = {.ino = current_ino};               // Inicializa o contexto de busca para o filho
        if (fs_iterate_dir(fs, &parent_inode, child_cb, &ctx) < 0) // Itera sobre o diretório pai para encontrar o nome do filho
            return NULL;

        components[count++] = strdup(ctx.name); // Armazena o nome do filho encontrado
        current_ino = parent_ino;               // Atualiza o inode atual para o pai
    }

    // Calcula o tamanho do caminho
    size_t total_len = 1;
    for (int i = count - 1; i >= 0; --i)
        total_len += strlen(components[i]) + 1;

    char *path = malloc(total_len + 1);
    if (!path)
        return NULL;

    char *ptr = path; // Ponteiro para construir o caminho
    *ptr++ = '/';
    for (int i = count - 1; i >= 0; --i)
    {
        size_t len = strlen(components[i]);
        memcpy(ptr, components[i], len); // Copia o componente do caminho
        ptr += len;
        if (i)
            *ptr++ = '/';
        free(components[i]);
    }
    *ptr = '\0'; // Garante que o caminho seja null-terminated
    return path;
}

/**
 * @brief   Junta um caminho relativo com o diretório atual.
 *
 * Esta função combina um caminho relativo com o caminho do diretório atual,
 * retornando o caminho completo como uma string alocada dinamicamente.
 *
 * @param fs   Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param cwd  Número do inode do diretório atual.
 * @param rel  Caminho relativo a ser combinado.
 *
 * @return Retorna o caminho completo como uma string alocada dinamicamente, ou NULL em caso de erro.
 */
char *fs_join_path(ext2_fs_t *fs, uint32_t cwd, const char *rel)
{
    if (!rel || !*rel)
        return NULL;

    // Se o caminho relativo já é absoluto, apenas duplica
    if (rel[0] == '/')
        return strdup(rel);

    // Obtém o caminho absoluto do diretório atual
    char *base = fs_get_path(fs, cwd);
    if (!base)
        return NULL;

    size_t base_len = strlen(base);
    size_t rel_len = strlen(rel);

    // Verifica se precisa adicionar uma barra entre base e rel
    int add_slash = (base_len > 1 && base[base_len - 1] != '/');

    // Aloca espaço para o novo caminho
    size_t total_len = base_len + add_slash + rel_len + 1;
    char *full = malloc(total_len);
    if (!full)
    {
        free(base);
        return NULL;
    }

    strcpy(full, base); // Copia o caminho base
    if (add_slash)
        strcat(full, "/");
    strcat(full, rel); // Adiciona o caminho relativo

    free(base);
    return full;
}

/**
 * @brief   Verifica se um nome já existe em um diretório.
 *
 * Esta função percorre as entradas de diretório e verifica se o nome fornecido
 * já está presente, retornando 1 se existir, 0 caso contrário.
 *
 * @param fs         Ponteiro para a estrutura do sistema de arquivos EXT2.
 * @param dir_inode  Ponteiro para o inode do diretório onde o nome será verificado.
 * @param name       Nome a ser verificado no diretório.
 *
 * @return Retorna 1 se o nome existir, 0 caso contrário, ou -1 em caso de erro.
 */
int name_exists(ext2_fs_t *fs, struct ext2_inode *dir_inode, char *name)
{
    if (!ext2_is_dir(dir_inode)) // Verifica se o inode é um diretório
    {
        return -1;
    }

    size_t name_len = strlen(name);

    // Percorre apenas os blocos diretos do diretório
    for (int i = 0; i < 12; ++i)
    {
        uint32_t block_num = dir_inode->i_block[i]; // Obtém o número do bloco
        if (!block_num)
            continue;

        uint8_t block_buf[EXT2_BLOCK_SIZE];
        if (fs_read_block(fs, block_num, block_buf) < 0) // Lê o bloco do diretório
            return -1;

        uint32_t offset = 0;
        while (offset < EXT2_BLOCK_SIZE)
        {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(block_buf + offset); // Obtém a entrada de diretório
            if (entry->rec_len == 0)
                break;

            if (entry->inode && entry->name_len == name_len && strncmp(entry->name, name, name_len) == 0) // Verifica se o nome corresponde
                return 1;
            offset += entry->rec_len; // Avança para a próxima entrada
        }
    }
    return 0;
}

/**
 * @brief   Sincroniza o superbloco do sistema de arquivos EXT2.
 *
 * Esta função escreve o superbloco atualizado de volta no disco, garantindo que as alterações
 * sejam persistidas. Utiliza a função pwrite para escrever os dados no offset do superbloco.
 *
 * @param fs  Ponteiro para a estrutura do sistema de arquivos EXT2.
 *
 * @return Retorna 0 em caso de sucesso, ou -1 em caso de erro na escrita.
 */
int fs_sync_super(ext2_fs_t *fs)
{
    ssize_t bytes_written = pwrite(fs->fd, &fs->sb, sizeof(fs->sb), EXT2_SUPER_OFFSET); // Escreve o superbloco no disco
    if (bytes_written == (ssize_t)sizeof(fs->sb))
        return 0;
    return -1;
}
