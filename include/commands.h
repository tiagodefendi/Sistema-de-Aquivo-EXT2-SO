#ifndef COMMANDS_H
#define COMMANDS_H

#include "utils.h"

/**
 * @file    commands.h
 *
 * Definições e protótipos de comandos do shell.
 *
 * @authors Artur Bento de Carvalho
 *          Eduardo Riki Matushita
 *          Rafaela Tieri Iwamoto Ferreira
 *          Tiago Defendi da Silva
 *
 * @date    01/07/2025
 */

/**
 * @brief   Função de comando.
 *
 * Esta função define a assinatura padrão de um comando no shell.
 *
 * @param   argc Número de argumentos passados para o comando.
 * @param   argv Array de strings com os argumentos do comando.
 * @param   fs Ponteiro para o sistema de arquivos aberto.
 * @param   cwd Ponteiro para o inode do diretório corrente.
 */
typedef int (*command_fn)(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);

struct command_entry
{
    const char *name;   // Nome do comando
    command_fn handler; // Função que implementa o comando
    const char *help;   // Descrição do comando
};

/* --------------- Comandos implementados --------------- */
int cmd_info(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_cat(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_attr(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_cd(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_ls(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_pwd(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_touch(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_mkdir(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_rm(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_rmdir(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_rename(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_cp(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);
int cmd_print(int argc, char **argv, ext2_fs_t *fs, uint32_t *cwd);

#define CMD_TABLE_END {NULL, NULL, NULL}

#endif /* COMMANDS_H */
