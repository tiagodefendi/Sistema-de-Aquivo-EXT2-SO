#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>
#include "ext2.h"

int cmd_info(FILE *img);
int cmd_ls(FILE *img, const char *path);
int cmd_cat(FILE *img, const char *cwd, const char *filename);
int cmd_pwd(const char *cwd);
int cmd_attr(FILE *img, const char *cwd, const char *target);
int cmd_print(FILE *img, const char *cwd, int argc, char **argv);
int cmd_cd(FILE *img, char *cwd, const char *path);

#endif
