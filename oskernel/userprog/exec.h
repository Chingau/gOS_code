#ifndef __GOX_EXEC_H__
#define __GOX_EXEC_H__
#include "types.h"

int32_t sys_execv(const char *path, const char *argv[]);
#endif
