#ifndef __GOS_OSKERNEL_GLOBAL_H__
#define __GOS_OSKERNEL_GLOBAL_H__

#define TI_GDT   0
#define TI_LDT   1
#define RPL0     0
#define RPL1     1
#define RPL2     2
#define PRL3     3

#define SELECTOR_K_CODE     ((1 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA     ((2 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_VIDEO    ((3 << 3) + (TI_GDT << 2) + RPL0)   //显存
#define SELECTOR_TSS        ((4 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_U_CODE     ((5 << 3) + (TI_GDT << 2) + PRL3)
#define SELECTOR_U_DATA     ((6 << 3) + (TI_GDT << 2) + PRL3)

#define SELECTOR_K_STACK    SELECTOR_K_DATA
#define SELECTOR_U_STACK    SELECTOR_U_DATA

#define DESC_TYPE_CODE  8   //x=1 c=0 r=0 a=0 可执行，非依从，不可读，已访问位a清0
#define DESC_TYPE_DATA  2   //x=0 e=0 w=1 a=0 不可执行，向上扩展，可写，已访问位a清0
#define DESC_TYPE_TSS   9   //B位为0，不忙

#define EFLAGS_MBS  (1 << 1)        //此项必须要设置
#define EFLAGS_IF_1 (1 << 9)        //if为1，开中断
#define EFLAGS_IF_0 0               //if为0，关中断
#define EFLAGS_IOPL_3   (3 << 12)   //IOPL3用于测试用户程序在非系统调用下进行IO
#define EFLAGS_IOPL_0   (0 << 12)

#define DIV_ROUND_UP(x, step)   ((x + step - 1) / (step))   //除法向上取整

#define PG_SIZE 4096

#endif
