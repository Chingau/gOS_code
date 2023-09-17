#ifndef __GOS_BUILDIN_CMD_H__
#define __GOS_BUILDIN_CMD_H__
#include "types.h"
#include "global.h"

void make_clear_abs_path(char *path, char *final_path);
int32_t buildin_pwd(uint32_t argc, char ** argv UNUSED);
int32_t buildin_cd(uint32_t argc, char **argv);
int32_t buildin_ls(uint32_t argc, char **argv);
int32_t buildin_ps(uint32_t argc, char **argv);
int32_t buildin_clear(uint32_t argc, char **argv);
int32_t buildin_mkdir(uint32_t argc, char **argv);
int32_t buildin_rmdir(uint32_t argc, char **argv);
int32_t buildin_rm(uint32_t argc, char **argv);
#endif