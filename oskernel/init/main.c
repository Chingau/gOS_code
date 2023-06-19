#include "kernel.h"
#include "traps.h"
#include "tty.h"
#include "mm.h"

extern void clock_init(void);

void kernel_main(void)
{
    int temp = 0x11223344;

    console_init();
    gdt_init();
    idt_init();
    clock_init();
    check_memory();

    printk("hello gos!\n");
    __asm__("sti;");
    while (1);
}
