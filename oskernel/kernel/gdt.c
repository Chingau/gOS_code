#include "system.h"
#include "kernel.h"
#include "head.h"
#include "string.h"

#define GDT_SIZE 256
gdt_item_t gdt[GDT_SIZE];
xdt_ptr_t gdt_ptr;
gdt_selector_t r3_code_selector;
gdt_selector_t r3_data_selector;

/*
* gdt_index: 段描述符在GDT表中的下标
* base: 段基址
* limit: 段界限
*/
static void create_code_gdt_item_r3(int gdt_index, int base, int limit)
{
    //实模式下已经创建了4个全局描述符，所以前4个不能再用了
    if (gdt_index < 4) {
        print_unlock("the gdt_index:%d has been used...\n", gdt_index);
        return;
    }

    gdt_item_t *item = (gdt_item_t *)&gdt[gdt_index];
    item->limit_low = limit & 0xffff;
    item->base_low = base & 0xffffff;
    item->type = 0b1000;
    item->segment = 1;
    item->dpl = 0b11;
    item->present = 1;
    item->limit_high = limit >> 16 & 0xf;
    item->available = 0;
    item->long_mode = 0;
    item->big = 1;
    item->granularity = 1;
    item->base_high = base >> 24 & 0xff;
}

/*
* gdt_index: 段描述符在GDT表中的下标
* base: 段基址
* limit: 段界限
*/
static void create_data_gdt_item_r3(int gdt_index, int base, int limit)
{
    //实模式下已经创建了4个全局描述符，所以前4个不能再用了
    if (gdt_index < 4) {
        print_unlock("the gdt_index:%d has been used...\n", gdt_index);
        return;
    }

    gdt_item_t* item = (gdt_item_t *)&gdt[gdt_index];
    item->limit_low = limit & 0xffff;
    item->base_low = base & 0xffffff;
    item->type = 0b0010;
    item->segment = 1;
    item->dpl = 0b11;
    item->present = 1;
    item->limit_high = limit >> 16 & 0xf;
    item->available = 0;
    item->long_mode = 0;
    item->big = 1;
    item->granularity = 1;
    item->base_high = base >> 24 & 0xff;
}

void gdt_init(void)
{
    print_unlock("init gdt...\n");

    __asm__ volatile ("sgdt gdt_ptr;");
    memcpy(&gdt, (char *)gdt_ptr.base, gdt_ptr.limit);

    //创建r3用的段描述符：代码段，数据段
    create_code_gdt_item_r3(4, 0, 0xfffff);
    create_data_gdt_item_r3(5, 0, 0xfffff);

    //创建r3用的选择子：代码段，数据段
    r3_code_selector.RPL = 3;
    r3_code_selector.TI = 0;
    r3_code_selector.index = 4;
    r3_data_selector.RPL = 3;
    r3_data_selector.TI = 0;
    r3_data_selector.index = 5;

    gdt_ptr.base = (int)gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;

    BOCHS_DEBUG_MAGIC
    __asm__("lgdt gdt_ptr;");
}