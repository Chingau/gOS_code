#ifndef __GOS_OSKERNEL_SYSCALL_H
#define __GOS_OSKERNEL_SYSCALL_H
#include "types.h"

enum SYSCALL_NR {
    SYS_GETPID = 0,
    SYS_WRITE = 1,
    SYS_MAX_NR
};

uint32_t getpid(void);
uint32_t write(char *str);

#endif
