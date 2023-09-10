#include "syscall.h"

/* 无参数的系统调用 */
#define _syscall0(NUMBER)   ({  \
    int retval;                 \
    __asm__ volatile (          \
    "int 0x80"                 \
    : "=a"(retval)              \
    : "a"(NUMBER)               \
    : "memory");                \
    retval;                     \
})

/* 一个参数的系统调用 */
#define _syscall1(NUMBER, ARG1) ({  \
    int retval;                                 \
    __asm__ volatile (                          \
    "int 0x80"                                 \
    : "=a"(retval)                              \
    : "a"(NUMBER), "b"(ARG1)                    \
    : "memory");                                \
    retval;                                     \
})

/* 二个参数的系统调用 */
#define _syscall2(NUMBER, ARG1, ARG2) ({  \
    int retval;                                 \
    __asm__ volatile (                          \
    "int 0x80"                                 \
    : "=a" (retval)                             \
    : "a" (NUMBER), "b"(ARG1), "c"(ARG2)        \
    : "memory");                                \
    retval;                                     \
})

/* 三个参数的系统调用 */
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({  \
    int retval;                                 \
    __asm__ volatile (                          \
    "int 0x80"                                 \
    : "=a" (retval)                             \
    : "a" (NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3) \
    : "memory");                                \
    retval;                                     \
})

uint32_t getpid(void)
{
    return _syscall0(SYS_GETPID);
}

//把buf中count个字符写入文件描述符fd
uint32_t write(int32_t fd, const void *buf, uint32_t count)
{
    return _syscall3(SYS_WRITE, fd, buf, count);
}

/* 申请size字节大小的内存，返回分配地址的首地址 */
void *malloc(uint32_t size)
{
    return (void *)_syscall1(SYS_MALLOC, size);
}

/* 释放ptr指向的内存 */
void free(void *ptr)
{
    _syscall1(SYS_FREE, ptr);
}