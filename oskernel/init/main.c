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

    intr_enable();
    while (1);
    //BOCHS_DEBUG_MAGIC
}


void k_thread_a(void *arg)
{
    char *para = arg;
    void *addr = sys_malloc(33);
    printk("%s pid:%d, sys_malloc addr:%08x\n", para, sys_getpid(), (int)addr);
    while (1);
}

void k_thread_b(void *arg)
{
    char *para = arg;
    void *addr = sys_malloc(63);
    printk("%s pid:%d, sys_malloc addr:%08x\n", para, sys_getpid(), (int)addr);
    while (1);
}
