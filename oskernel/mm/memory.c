/*
 * Create by gaoxu on 2023.06.11
 * */
#include "kernel.h"
#include "mm.h"

#define ARDS_ADDR   0x1100
#define LOW_MEM     0x100000    //1M以下的物理内在给内核用

#define ZONE_VAILD      1       //ards可用内存区域
#define ZONE_RESERVED   2       //ards不可用内存区域

void check_memory(void)
{
    check_memory_info_t *p = (check_memory_info_t *)ARDS_ADDR;
    check_memory_item_t *ards = (check_memory_item_t *)(ARDS_ADDR + 2);
    unsigned short times = p->times;
    check_memory_item_t *temp;

    printk("BaseAddrHigh BaseAddrLow LengthHigh  LengthLow   Type\n");
    for (int i = 0; i < times; ++i) {
        temp = (ards + i);
        printk("0x%08x,  0x%08x, 0x%08x, 0x%08x, %d\n",
               temp->base_addr_high, temp->base_addr_low, temp->length_high, temp->length_low, temp->type);
    }
}
