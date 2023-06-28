#include "kernel.h"
#include "traps.h"
#include "tty.h"
#include "mm.h"

extern void clock_init(void);

void kernel_main(void)
{
    int *addr;

    console_init();
    gdt_init();
    idt_init();
    clock_init();
    check_memory();
    memory_map_init();

    addr = get_free_page();
    get_free_page();
    get_free_page();
    free_page(addr);

    printk("hello gos!\n");
    __asm__("sti;");
    while (1);
}
