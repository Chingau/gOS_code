#include "kernel.h"
#include "interrupt.h"
#include "types.h"

char *messages[] = {
        "#DE Divide Error\0",
        "#DB RESERVED\0",
        "--  NMI Interrupt\0",
        "#BP Breakpoint\0",
        "#OF Overflow\0",
        "#BR BOUND Range Exceeded\0",
        "#UD Invalid Opcode (Undefined Opcode)\0",
        "#NM Device Not Available (No Math Coprocessor)\0",
        "#DF Double Fault\0",
        "    Coprocessor Segment Overrun (reserved)\0",
        "#TS Invalid TSS\0",
        "#NP Segment Not Present\0",
        "#SS Stack-Segment Fault\0",
        "#GP General Protection\0",
        "#PF Page Fault\0",
        "--  (Intel reserved. Do not use.)\0",
        "#MF x87 FPU Floating-Point Error (Math Fault)\0",
        "#AC Alignment Check\0",
        "#MC Machine Check\0",
        "#XF SIMD Floating-Point Exception\0",
        "#VE Virtualization Exception\0",
        "#CP Control Protection Exception\0",
};

void exception_handler(int idt_index,
                       int edi,
                       int esi,
                       int ebp,
                       int esp,
                       int ebx,
                       int edx,
                       int ecx,
                       int eax,
                       int eip,
                       char cs,
                       int eflags)
{
    printk("\n===============\n");
    printk("EXCEPTION : %s \n", messages[idt_index]);
    printk("   VECTOR : 0x%02X\n", idt_index);
    printk("   EFLAGS : 0x%08X\n", eflags);
    printk("       CS : 0x%02X\n", cs);
    printk("      EIP : 0x%08X\n", eip);
    printk("      ESP : 0x%08X\n", esp);
    printk("===============\n");
    while(1);
}


#define EFLAGS_IF   0x00000200  //eflags寄存器中的if位为1
#define GET_EFLAGS(flags_var)   __asm__ volatile("pushf; pop %0" : "=g"(flags_var))

/* 获取当前中断状态 */
INTR_STATUS_T intr_get_status(void)
{
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (eflags & EFLAGS_IF) ? INTR_ON : INTR_OFF;
}

/* 将中断状态设置为入参 status */
INTR_STATUS_T intr_set_status(INTR_STATUS_T status)
{
    return (status & INTR_ON) ? intr_enable() : intr_disable();
}

/* 开中断并返回开中断前的中断状态 */
INTR_STATUS_T intr_enable(void)
{
    INTR_STATUS_T old_status = intr_get_status();
    if (old_status == INTR_OFF) {
        __asm__ volatile("sti");
    }
    return old_status;
}

/* 关中断并返回开中断前的中断状态 */
INTR_STATUS_T intr_disable(void)
{
    INTR_STATUS_T old_status = intr_get_status();
    if (old_status == INTR_ON) {
        __asm__ volatile("cli":::"memory");
    }
    return old_status;
}
