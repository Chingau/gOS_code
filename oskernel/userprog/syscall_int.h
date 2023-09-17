#ifndef __GOS_OSKERNEL_SYSCALL_INT_H
#define __GOS_OSKERNEL_SYSCALL_INT_H
#include "types.h"

extern void cls_screen(void);

uint32_t sys_getpid(void);
void syscall_init(void);

#endif
