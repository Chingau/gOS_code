#include "head.h"
#include "kernel.h"
#include "types.h"

#define INTERRUPT_TABLE_SIZE 256
interrupt_gate_t idt[INTERRUPT_TABLE_SIZE] = {0};
xdt_ptr_t idt_ptr;

extern int interrupt_handler_table[0x2f];   //在汇编中定义的中断向量表
extern void interrupt_default_entry(void);
extern void keymap_handler_entry(void);

void idt_init(void)
{
    printk("init idt...\n");

    interrupt_gate_t *item = NULL;
    int handler;

    for (int index = 0; index < INTERRUPT_TABLE_SIZE; ++index) {
        item = (interrupt_gate_t *)&idt[index];
        if (index <= 0x15) {
            //不可屏蔽中断
            handler = (int)interrupt_handler_table[index];
        } else {
            //可屏蔽中断
            handler = (int)interrupt_default_entry;
        }

        //键盘中断
        if (index == 0x21) {
            handler = (int)keymap_handler_entry;
        }

        item->offset0 = handler & 0xffff;
        item->offset1 = (handler >> 16) & 0xffff;
        item->selector = 1 << 3;     //代码段
        item->reserved = 0;
        item->type = 0b1110;        //中断门
        item->segment = 0;          //系统段
        item->dpl = 0;              //内核态
        item->present = 1;           //有效
    }

    idt_ptr.base = (int)idt;
    idt_ptr.limit = sizeof(idt) - 1;

    __asm__("lidt idt_ptr;");
}