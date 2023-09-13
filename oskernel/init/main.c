#include "kernel.h"
#include "traps.h"
#include "string.h"
#include "tty.h"
#include "mm.h"
#include "system.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"
#include "timer.h"
#include "keyboard.h"
#include "tss.h"
#include "userprog.h"
#include "syscall_int.h"
#include "syscall.h"
#include "stdio.h"
#include "ide.h"
#include "fs.h"
#include "dir.h"

void k_thread_a(void *);
void k_thread_b(void *);
void u_prog_a(void);
void u_prog_b(void);

void kernel_main(void)
{
    console_init();
    tss_init();
    idt_init();
    check_memory();
    mem_init();
    timer_init();
    keyboard_init();
    thread_init();
    console_lock_init();
    syscall_init();
    ide_init();
    filesys_init();
    print_unlock("hello gos, init done.\n");
    BOCHS_DEBUG_MAGIC
    
    //创建两个内核线程
    thread_start("consumer_a", 10, k_thread_a, "argA");
    thread_start("consumer_b", 10, k_thread_b, "argB");

    process_execute(u_prog_a, "u_prog_a", 20);
    process_execute(u_prog_b, "u_prog_b", 20);

    intr_enable();

    // printf("/dir2/subdir2 create %s.\n", sys_mkdir("/dir2/subdir2") == 0 ? "done" : "fail");
    // printf("/dir2 create %s.\n", sys_mkdir("/dir2") == 0 ? "done" : "fail");
    // printf("now, /dir2/subdir2 create %s.\n", sys_mkdir("/dir2/subdir2") == 0 ? "done" : "fail");

    char cwd_buf[32] = {0};
    sys_getcwd(cwd_buf, 32);
    printf("cwd:%s\n", cwd_buf);
    sys_chdir("/dir1");
    sys_getcwd(cwd_buf, 32);
    printf("cwd:%s\n", cwd_buf);
    
__exit:
    while (1);
    //BOCHS_DEBUG_MAGIC
}

void k_thread_a(void *arg)
{
    while (1);
}

void k_thread_b(void *arg)
{
    while (1);
}

void u_prog_a(void)
{
    while (1);
}

void u_prog_b(void)
{
    while (1);
}