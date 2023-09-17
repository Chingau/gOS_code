#include "thread.h"
#include "kernel.h"
#include "syscall.h"
#include "types.h"
#include "string.h"
#include "fs.h"
#include "fork.h"
#include "syscall_int.h"
#include "tty.h"

typedef void* syscall;
syscall syscall_table[SYS_MAX_NR];

/*
 * 返回当前任务的pid
*/
uint32_t sys_getpid(void)
{
    return running_thread()->pid;
}

void sys_putchar(char char_asci)
{
    printk("%c", char_asci);
}

/*
 * 初始化系统调用表
*/
void syscall_init(void)
{
    print_unlock("syscall init...\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    syscall_table[SYS_FORK] = sys_fork;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_CLEAR] = console_clear;
    syscall_table[SYS_PUTCHAR] = sys_putchar;
}