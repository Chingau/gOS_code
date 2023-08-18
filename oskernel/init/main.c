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

void k_thread_a(void *);
void k_thread_b(void *);
void u_prog_a(void);
void u_prog_b(void);

int test_var_a = 0;
int test_var_b = 0;

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
    while (1);
    //BOCHS_DEBUG_MAGIC
}


void k_thread_a(void *arg)
{
    char *para = arg;

    while (1) {
        printk("%s:0x%08x\n", para, test_var_a);
    }
}

void k_thread_b(void *arg)
{
    char *para = arg;

    while (1) {
        printk("%s:0x%08x\n", para, test_var_b);
    }
}

//测试用户进程
void u_prog_a(void)
{
    while (1) {
        test_var_a = getpid();;
    }
}

void u_prog_b(void)
{
    while (1) {
        test_var_b = getpid();;
    }
}