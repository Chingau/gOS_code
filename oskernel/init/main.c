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

    process_execute(u_prog_a, "u_prog_a", 20);
    process_execute(u_prog_b, "u_prog_b", 20);

    intr_enable();
    while (1);
    //BOCHS_DEBUG_MAGIC
}

/*
    两个内核线程分配的内存是累加的，因为它们是共用内核内存，分配的地址如下：
    argA addr1:c013400c addr2:c013410c addr3:c013420c       (256=0x100)
    argB 地址累加：
    argB addr1:c013430c addr2:c013440c addr3:c013450c

    而两个用户进程，因为地址空间独立，所以分配的地址都是一样的
    u-a addr1:0804800c addr2:0804810c addr3:0804820c
    u-b addr1:0804800c addr2:0804810c addr3:0804820c
*/

void k_thread_a(void *arg)
{
    char *para = arg;
    void *addr1 = sys_malloc(256);
    void *addr2 = sys_malloc(255);
    void *addr3 = sys_malloc(256);
    printk("[%s]addr1:%08x, addr2:%08x, addr3:%08x\r\n", para, (int)addr1, (int)addr2, (int)addr3);

    //int cpu_delay = 100000;
    //while (cpu_delay-- > 0);
    sys_free(addr1);
//    sys_free(addr2);
//    sys_free(addr3);
    while (1);
}

void k_thread_b(void *arg)
{
    char *para = arg;
    void *addr1 = sys_malloc(256);
    void *addr2 = sys_malloc(255);
    void *addr3 = sys_malloc(256);
    printk("[%s]addr1:%08x, addr2:%08x, addr3:%08x\r\n", para, (int)addr1, (int)addr2, (int)addr3);

    int cpu_delay = 100000;
    while (cpu_delay-- > 0);
    sys_free(addr1);
    sys_free(addr2);
    sys_free(addr3);
    while (1);
}

void u_prog_a(void)
{
    void *addr1 = malloc(256);
    void *addr2 = malloc(255);
    void *addr3 = malloc(256);
    printf("[u-a]addr1:%08x, addr2:%08x, addr3:%08x\r\n", (int)addr1, (int)addr2, (int)addr3);

    int cpu_delay = 100000;
    while (cpu_delay-- > 0);
    free(addr1);
    free(addr2);
    free(addr3);
    while (1);
}

void u_prog_b(void)
{
    void *addr1 = malloc(256);
    void *addr2 = malloc(255);
    void *addr3 = malloc(256);
    printf("[u-b]addr1:%08x, addr2:%08x, addr3:%08x\r\n", (int)addr1, (int)addr2, (int)addr3);

    int cpu_delay = 100000;
    while (cpu_delay-- > 0);
    free(addr1);
    free(addr2);
    free(addr3);
    while (1);
}