#include "kernel.h"
#include "interrupt.h"
#include "types.h"
#include "head.h"
#include "io.h"

#define INTERRUPT_TABLE_SIZE (0x2f + 1)

extern int interrupt_handler_table[INTERRUPT_TABLE_SIZE];

interrupt_gate_t idt[INTERRUPT_TABLE_SIZE] = {0};
xdt_ptr_t idt_ptr;
char *intr_name[INTERRUPT_TABLE_SIZE];      //中断处理函数名
int intr_table[INTERRUPT_TABLE_SIZE];     //中断处理函数

/* 缺省的中断处理程序 */
void general_intr_handler(int vec_nr)
{
    //伪中断，不关心
    if (vec_nr == 0x27 || vec_nr == 0x2f) {
        return;
    }

    print_unlock("VECTOR: 0x%02x\r\n", vec_nr);
    while(1);
}

/* 不可屏蔽中断发生时的中断处理函数 */
static void exception_handler(int vec_nr,
                       int edi,
                       int esi,
                       int ebp,
                       int esp,
                       int ebx,
                       int edx,
                       int ecx,
                       int eax,
                       int error_code,
                       int eip,
                       char cs,
                       int eflags)
{
    print_unlock("\n===============\n");
    print_unlock("EXCEPTION : %s \n", intr_name[vec_nr]);
    print_unlock("   VECTOR : 0x%02X\n", vec_nr);
    print_unlock("   EFLAGS : 0x%08X\n", eflags);
    print_unlock("       CS : 0x%02X\n", cs);
    print_unlock("      EIP : 0x%08X\n", eip);
    print_unlock("      ESP : 0x%08X\n", esp);
    print_unlock(" ERR_CODE : 0x%08X\n", error_code);
    print_unlock("===============\n");
    while(1);
}

static void intr_init(void)
{
    int i;

    for (i = 0; i < INTERRUPT_TABLE_SIZE; ++i) {
        if (i <= 21) {
            intr_table[i] = (int)exception_handler;
        } else {
            intr_table[i] = (int)general_intr_handler;
            intr_name[i] = "unknown";
        }
    }

    intr_name[0] = "#DE Divide Error\0";
    intr_name[1] = "#DB RESERVED\0";
    intr_name[2] = "NMI Interrupt\0";
    intr_name[3] = "#BP Breakpoint\0";
    intr_name[4] = "#OF Overflow\0";
    intr_name[5] = "#BR BOUND Range Exceeded\0";
    intr_name[6] = "#UD Invalid Opcode (Undefined Opcode)\0";
    intr_name[7] = "#NM Device Not Available (No Math Coprocessor)\0";
    intr_name[8] = "#DF Double Fault\0";
    intr_name[9] =  "Coprocessor Segment Overrun (reserved)\0";
    intr_name[10] = "#TS Invalid TSS\0";
    intr_name[11] = "#NP Segment Not Present\0";
    intr_name[12] = "#SS Stack-Segment Fault\0";
    intr_name[13] = "#GP General Protection\0";
    intr_name[14] = "#PF Page Fault\0";
    intr_name[15] = "(Intel reserved. Do not use.)\0";
    intr_name[16] = "#MF x87 FPU Floating-Point Error (Math Fault)\0";
    intr_name[17] = "#AC Alignment Check\0";
    intr_name[18] = "#MC Machine Check\0";
    intr_name[19] = "#XF SIMD Floating-Point Exception\0";
    intr_name[20] = "#VE Virtualization Exception\0";
    intr_name[21] = "#CP Control Protection Exception\0";
}

/* idt表初始化 */
static void idt_desc_init(void)
{
    print_unlock("init idt desc...\n");

    interrupt_gate_t *item = NULL;
    int handler;

    for (int index = 0; index < INTERRUPT_TABLE_SIZE; ++index) {
        item = (interrupt_gate_t *)&idt[index];
        handler = interrupt_handler_table[index];
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

#define PIC_M_CTRL 0x20     //主片控制端口
#define PIC_M_DATA 0x21     //主片数据端口
#define PIC_S_CTRL 0xA0     //从片控制端口
#define PIC_S_DATA 0xA1     //从片数据端口

/* 初始化可编程中断控制器8259A */
static void pic_init(void)
{
    print_unlock("pic init.\r\n");

    /* 初始化主片 */
    outb(PIC_M_CTRL, 0x11); //ICW1:边沿触发，级联8259，需要ICW4
    outb(PIC_M_DATA, 0x20); //ICW2:起始中断向量号为0x20，也就是IR[0-7]为0x20~0x27
    outb(PIC_M_DATA, 0x04); //ICW3:IR2接从片
    outb(PIC_M_DATA, 0x01); //ICW4:8086模式，正常   
    /* 初始化从片 */
    outb(PIC_S_CTRL, 0x11); //ICW1:边沿触发，级联8259，需要ICW4
    outb(PIC_S_DATA, 0x28); //ICW2:起始中断向量号为0x28，也就是IR[8-15]为0x28~0x2F
    outb(PIC_S_DATA, 0x02); //ICW3:设置从片连接到主片的IR2引脚
    outb(PIC_S_DATA, 0x01); //ICW4:8086模式，正常   
    //打开主片上IR0,也就是目前只接受时钟产生的中断
    outb(PIC_M_DATA, 0xfe); //0xfd只开键盘中断，0xfe只开时钟中断，0xfc键盘/时钟中断都打开
    outb(PIC_S_DATA, 0xff);
}


void register_handler(uint8_t vector_no, void *function)
{
     intr_table[vector_no] = (int)function;
}

void idt_init(void)
{
    intr_init();
    idt_desc_init();
    pic_init();
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
