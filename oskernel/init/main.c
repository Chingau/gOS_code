#include "kernel.h"
#include "traps.h"
#include "tty.h"
#include "mm.h"
#include "system.h"
#include "thread.h"
#include "debug.h"

extern void clock_init(void);

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
    uint32_t *addr = (uint32_t *)get_kernel_pages(3);
    printk("malloc addr: 0x%08x\r\n", addr);
    __asm__("sti;");
    ASSERT(1==2);       //测试断言

    __asm__("sti;");
    BOCHS_DEBUG_MAGIC
    while (1);
}