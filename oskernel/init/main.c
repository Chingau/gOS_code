#include "kernel.h"
#include "traps.h"
#include "tty.h"
#include "mm.h"

extern void clock_init(void);

void kernel_main(void)
{
    console_init();
    gdt_init();
    idt_init();
    clock_init();
    check_memory();
    memory_map_init();
    virtual_memory_init();

    // 测试分配虚拟内存
    void* p = kmalloc(1);
    printk("kmalloc: 0x%p\n", p);
    kfree_s(p, 1);

    kmalloc(100);

    printk("hello gos!\n");
    __asm__("sti;");
    while (1);
}
