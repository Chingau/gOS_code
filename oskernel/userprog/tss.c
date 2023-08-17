#include "tss.h"
#include "head.h"
#include "global.h"
#include "string.h"
#include "kernel.h"
#include "system.h"

/* 任务状态段结构 */
struct tss {
    uint32_t backlink;
    uint32_t *esp0;
    uint32_t ss0;
    uint32_t *esp1;
    uint32_t ss1;
    uint32_t *esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t (*eip)(void);
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint32_t trace;
    uint32_t io_base;
};
static struct tss tss;

/* 更新tss中esp0字段的值为pthread的0级栈 */
void update_tss_esp(struct task_struct *pthread)
{
    tss.esp0 = (uint32_t *)((uint32_t)pthread + PG_SIZE);
}

/*
* base: 段基址
* limit: 段界限
* type: 段类型(写读执行)
* segment: 1-非系统段(data/code) 0-系统段(tss及各种门)
* dpl: 特权级
* d_b: 对应D/B位
* g: 粒度 0-1B 1-4KB
*/
static gdt_item_t make_gdt_desc(uint32_t base, uint32_t limit, char type, char segment, char dpl, char d_b, char g)
{
    gdt_item_t item = {0};

    item.limit_low = limit & 0xffff;
    item.base_low = base & 0xffffff;
    item.type = type;
    item.segment = segment;
    item.dpl = dpl;
    item.present = 1;
    item.limit_high = limit >> 16 & 0xf;
    item.available = 0;
    item.long_mode = 0;
    item.big = d_b;
    item.granularity = g;
    item.base_high = base >> 24 & 0xff;

    return item;
}

void tss_init(void)
{
    print_unlock("tss init.\r\n");

    uint32_t tss_size = sizeof(tss);
    xdt_ptr_t gdt_ptr;

    memset(&tss, 0, tss_size);
    tss.ss0 = SELECTOR_K_STACK;
    tss.io_base = tss_size; //无io位图

    // GDT基址为0x500，把tss放到第4个位置
    *((gdt_item_t *)0xc0000520) = make_gdt_desc((uint32_t)&tss, tss_size-1, DESC_TYPE_TSS, 0, 0, 0, 1);
    // 第5个段存放用户代码段
    *((gdt_item_t *)0xc0000528) = make_gdt_desc(0, 0xfffff, DESC_TYPE_CODE, 1, 3, 1, 1);
    // 第6个段存放用户数据段
    *((gdt_item_t *)0xc0000530) = make_gdt_desc(0, 0xfffff, DESC_TYPE_DATA, 1, 3, 1, 1);

    gdt_ptr.base = 0xc0000500;
    gdt_ptr.limit = 8 * 7 - 1;  //目前已有7个段描述符
    __asm__ volatile("lgdt %0" :: "m"(gdt_ptr));
    __asm__ volatile("ltr %w0" :: "r"(SELECTOR_TSS));
}