#include "kernel.h"
#include "traps.h"
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
    print_unlock("hello gos!\n");

    //创建两个内核线程
    thread_start("consumer_a", 10, k_thread_a, "argA");
    thread_start("consumer_b", 10, k_thread_b, "argB");

    //创建两个用户进程
    process_execute(u_prog_a, "user_prog_a", 20);
    process_execute(u_prog_b, "user_prog_b", 5);

    intr_enable();
    printk("main pid:%d\n", sys_getpid());
    while (1);
    //BOCHS_DEBUG_MAGIC
}


void k_thread_a(void *arg)
{
    char *para = arg;

    printk("%s pid:%d\n", para, sys_getpid());
    while (1);
}

void k_thread_b(void *arg)
{
    char *para = arg;

    printk("%s pid:%d\n", para, sys_getpid());
    while (1);
}

//测试用户进程
void u_prog_a(void)
{
    printf("userA pid:%d\n", getpid());
    while (1);
}

void u_prog_b(void)
{
    printf("userB pid:%d\n", getpid());
    while (1);
}