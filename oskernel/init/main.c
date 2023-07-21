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

void k_thread_a(void *);
void k_thread_b(void *);

void kernel_main(void)
{
    console_init();
    gdt_init();
    idt_init();
    check_memory();
    mem_init();
    timer_init();
    keyboard_init();
    thread_init();
    console_lock_init();
    print_unlock("hello gos!\n");

    thread_start("consumer_a", 31, k_thread_a, "A_");
    thread_start("consumer_b", 31, k_thread_b, "B_");

    intr_enable();
    while (1);
    //BOCHS_DEBUG_MAGIC
}

/*
 主线程Main让其悬停；线程k_thread_a和线程k_thread_b作为键盘缓冲区的消费者，
 不断的从键盘缓冲区中取出字符打印到终端上，在输出它们之前先关中断，原因是ioq_getchar和
 ioq_putchar函数中的ASSERT(intr_get_status() == INTR_OFF).

 线程输出 A_  B_ 有利于分清是哪个线程从缓冲区中取走了数据，比如按住键盘k不松手，则会交替输出
 A_k B_k, A线程时间片用完后就会切到B线程；

 注意分析任务调度
*/


void k_thread_a(void *arg)
{
    char byte;
    INTR_STATUS_T old_stauts;

    while (1) {
        old_stauts = intr_disable();
        byte = ioq_getchar(&kbd_buf);
        printk("%s", arg);
        printk("%c ", byte);
        intr_set_status(old_stauts);
    }
}

void k_thread_b(void *arg)
{
    char byte;
    INTR_STATUS_T old_stauts;

    while (1) {
        old_stauts = intr_disable();
        byte = ioq_getchar(&kbd_buf);
        printk("%s", arg);
        printk("%c ", byte);
        intr_set_status(old_stauts);
    }
}