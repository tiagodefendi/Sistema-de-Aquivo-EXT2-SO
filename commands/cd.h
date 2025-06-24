#ifndef CMD_CD_H
#define CMD_CD_H

#include <stdint.h>

int cmd_cd(const char *dirname);
uint32_t get_current_inode();

#endif
