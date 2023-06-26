#include "kernel.h"
#include "traps.h"
#include "tty.h"
#include "mm.h"
#include "system.h"

extern void clock_init(void);

void kernel_main(void)
{
    int temp = 0x11223344;

    console_init();
    gdt_init();
    idt_init();
    clock_init();
    check_memory();
    mem_init();

    void *addr = get_kernel_pages(3);
    printk("get_kernel_page start vaddr is: 0x%08x\r\n", (uint32_t)addr);
    printk("hello gos!\n");
    __asm__("sti;");
    BOCHS_DEBUG_MAGIC
    while (1);
}
