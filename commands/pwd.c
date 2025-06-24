#include <stdio.h>
#include <string.h>
#include "pwd.h"

static char current_path[1024] = "/";

const char *get_current_path()
{
    return current_path;
}

char *get_current_path_mutable()
{
    return current_path;
}

void reset_path()
{
    strcpy(current_path, "/");
}

void cmd_pwd()
{
    printf("%s\n", current_path);
}
