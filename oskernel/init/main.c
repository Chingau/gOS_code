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
    //keyboard_init();
    thread_init();
    console_lock_init();
    
    print_unlock("hello gos!\n");

    thread_start("k_thread_a", 31, k_thread_a, "argA");
    thread_start("k_thread_b", 10, k_thread_b, "argB");
    intr_enable();
    while (1) {
        printk("Main\r\n");
    }
    //BOCHS_DEBUG_MAGIC
}

void k_thread_a(void *arg)
{
    char *para = arg;
    while (1) {
        printk("a: %s\r\n", para);
    }
}

void k_thread_b(void *arg)
{
    char *para = arg;
    while (1) {
        printk("b: %s\r\n", para);
    }
}