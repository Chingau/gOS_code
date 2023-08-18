#include "thread.h"
#include "kernel.h"
#include "syscall.h"
#include "types.h"

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
 * 初始化系统调用表
*/
void syscall_init(void)
{
    print_unlock("syscall init...\n");
    syscall_table[SYS_GETPID] = sys_getpid;
}