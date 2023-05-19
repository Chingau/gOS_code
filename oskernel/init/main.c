#include "kernel.h"
#include "traps.h"
#include "tty.h"

void kernel_main(void)
{
    console_init();
    gdt_init();

    printk("hello gos!\n");
    while (1);
}
