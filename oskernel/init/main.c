#include "kernel.h"
#include "traps.h"
#include "tty.h"
#include "mm.h"
#include "system.h"
#include "thread.h"

extern void clock_init(void);

void test_thread_proc(void *arg)
{
    int para = *(int *)arg;

    for (int i = 0; i < para; ++i) {
        printk("thread: %d\r\n", i);
    }
    return;
}

void kernel_main(void)
{
    int num = 10;

    console_init();
    gdt_init();
    idt_init();
    clock_init();
    check_memory();
    mem_init();

    printk("hello gos!\n");

    thread_start("test_thread", 1, test_thread_proc, &num);

    __asm__("sti;");
    BOCHS_DEBUG_MAGIC
    while (1);
}