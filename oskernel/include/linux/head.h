/*
 * Create by gaoxu on 2023.05.19
 * */
#ifndef __GOS_OSKERNEL_HEAD_H__
#define __GOS_OSKERNEL_HEAD_H__

typedef struct {
    unsigned short limit_low;           // 段界限 bit0~bit15
    unsigned int base_low : 24;         // 段基址 bit16~bit39
    unsigned char type : 4;             // 段类型 bit40~bit43（系统段，非系统段）
    unsigned char segment : 1;          // S bit44 1-非系统段(代码段或数据段) 0-系统段
    unsigned char dpl : 2;              // DPL bit45~bit46 描述符特权级0~3
    unsigned char present : 1;          // P bit47 存在位 1-在内存 0-在磁盘
    unsigned char limit_high : 4;       // 段界限 bit48~bit51
    unsigned char available : 1;        // AVL bit52 保留
    unsigned char long_mode : 1;        // L bit53 1-64位代码段 0-32位代码段
    unsigned char big : 1;              // D/B bit54 0-16位 1-32位
    unsigned char granularity : 1;      // G bit55 0-粒度1B 1-粒度4KB
    unsigned char base_high;            // 段基址 bit56~bit63
} __attribute__((packed)) gdt_item_t;

typedef struct {
    char RPL : 2;           //请求特权级(当前特权级)
    char TI : 1;            //0-在GDT表索引 1-在LDT表索引
    short index : 13;       //索引值，即GDT/LDT的下标
}__attribute__((packed)) gdt_selector_t;

#pragma pack(2)
// GDT/LDT 表
typedef struct {
    short limit;
    int base;       //这个base需要大端存储，默认是小端存储
} xdt_ptr_t;
#pragma pack()

#endif
