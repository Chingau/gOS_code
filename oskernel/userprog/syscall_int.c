#include "thread.h"
#include "kernel.h"
#include "syscall.h"
#include "types.h"
#include "string.h"

#define syscall_nr  32
typedef void* syscall;
syscall syscall_table[syscall_nr];

/*
 * 返回当前任务的pid
*/
uint32_t sys_getpid(void)
{
    return running_thread()->pid;
}

/*
 * 打印字符串str(未实现文件系统前的版本)
*/
uint32_t sys_write(char *str)
{
    printk(str);
    return strlen((const char *)str);
}

/*
 * 初始化系统调用表
*/
void syscall_init(void)
{
    print_unlock("syscall init...\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
}