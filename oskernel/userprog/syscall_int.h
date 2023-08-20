#ifndef __GOS_OSKERNEL_SYSCALL_INT_H
#define __GOS_OSKERNEL_SYSCALL_INT_H
#include "types.h"

uint32_t sys_getpid(void);
uint32_t sys_write(char *str);
void syscall_init(void);

#endif
