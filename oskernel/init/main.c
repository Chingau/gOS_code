#include "kernel.h"
#include "traps.h"
#include "tty.h"

void kernel_main(void)
{
    console_init();
    gdt_init();
    idt_init();

    printk("hello gos!\n");
    __asm__("sti;");
    while (1);
}
