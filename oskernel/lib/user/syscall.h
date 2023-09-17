#ifndef __GOS_OSKERNEL_SYSCALL_H
#define __GOS_OSKERNEL_SYSCALL_H
#include "types.h"

enum SYSCALL_NR {
    SYS_GETPID      = 0,
    SYS_WRITE       = 1,
    SYS_MALLOC      = 2,
    SYS_FREE        = 3,
    SYS_FORK        = 4,
    SYS_READ        = 5,
    SYS_CLEAR       = 6,
    SYS_PUTCHAR     = 7,
    SYS_MAX_NR
};

uint32_t getpid(void);
uint32_t write(int32_t fd, const void *buf, uint32_t count);
void *malloc(uint32_t size);
void free(void *ptr);
pid_t fork(void);
ssize_t read(int fd, void *buf, size_t count);
void clear(void);
void putchar(char char_asci);
#endif
