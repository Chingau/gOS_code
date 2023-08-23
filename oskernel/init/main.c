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

/*
    当两个线程运行完后，内核的位图值前后不变才是正常的
*/

void k_thread_a(void *arg)
{
    char *para = arg;
    void *addr1;
    void *addr2;
    void *addr3;
    void *addr4;
    void *addr5;
    void *addr6;
    void *addr7;
    int max = 1000;

    printk("%s pid:%d, sys_malloc addr:%08x\n", para, sys_getpid());
    while (max-- > 0) {
        int size = 128;
        addr1 = sys_malloc(size);
        size *= 2;
        addr2 = sys_malloc(size);
        size *= 2;
        addr3 = sys_malloc(size);
        sys_free(addr1);
        addr4 = sys_malloc(size);
        size *= 2;
        size *= 2;
        size *= 2;
        size *= 2;
        size *= 2;
        size *= 2;
        size *= 2;
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);
        sys_free(addr5);
        size *= 2;
        addr7 = sys_malloc(size);
        sys_free(addr7);
        sys_free(addr6);
        sys_free(addr4);
        sys_free(addr3);
        sys_free(addr2);
    }
    printk("thread a end.\n");
    while (1);
}

void k_thread_b(void *arg)
{
    char *para = arg;
    void *addr1;
    void *addr2;
    void *addr3;
    void *addr4;
    void *addr5;
    void *addr6;
    void *addr7;
    void *addr8;
    int max = 100;

    printk("%s pid:%d, sys_malloc addr:%08x\n", para, sys_getpid());
    while (max-- > 0) {
        int size = 9;
        addr1 = sys_malloc(size);
        size *= 2;
        size *= 2;
        size *= 2;
        addr2 = sys_malloc(size);
        size *= 2;
        size *= 2;
        size *= 2;
        addr3 = sys_malloc(size);
        size *= 2;
        size *= 2;
        size *= 2;
        addr4 = sys_malloc(size);
        addr5 = sys_malloc(size);
        size *= 2;
        size *= 2;
        size *= 2;
        addr6 = sys_malloc(size);
        sys_free(addr1);
        sys_free(addr2);
        sys_free(addr3);
        sys_free(addr4);
        sys_free(addr5);
        sys_free(addr6);
        size *= 2;
        size *= 2;
        size *= 2;
        addr7 = sys_malloc(size);
        addr8 = sys_malloc(size);
        sys_free(addr7);
        sys_free(addr8);
    }  
    printk("thread b end.\n");
    while (1);
}
