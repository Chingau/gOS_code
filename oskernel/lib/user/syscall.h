#ifndef __GOS_OSKERNEL_SYSCALL_H
#define __GOS_OSKERNEL_SYSCALL_H
#include "types.h"

enum SYSCALL_NR {
    SYS_GETPID      = 0,
    SYS_WRITE       = 1,
    SYS_MALLOC      = 2,
    SYS_FREE        = 3,
    SYS_MAX_NR
};

uint32_t getpid(void);
uint32_t write(int32_t fd, const void *buf, uint32_t count);
void *malloc(uint32_t size);
void free(void *ptr);

#endif
