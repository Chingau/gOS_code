/*
 * Create by gaoxu on 2023.06.11
 * */
#include "kernel.h"
#include "mm.h"
#include "string.h"
#include "types.h"
#include "system.h"

#define ARDS_ADDR   0x1100
#define LOW_MEM     0x100000    //1M以下的物理内在给内核用

#define ZONE_VAILD      1       //ards可用内存区域
#define ZONE_RESERVED   2       //ards不可用内存区域

physics_memory_inof_t g_physics_memory;
physics_memory_map_t g_physics_memory_map;

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

        if (temp->base_addr_low == LOW_MEM && temp->type == ZONE_VAILD) {
            g_physics_memory.addr_start = temp->base_addr_low;
            g_physics_memory.valid_mem_size = temp->length_low;
            g_physics_memory.addr_end = temp->base_addr_low + temp->length_low;
        }
    }

    //如果没有可用内存
    if (g_physics_memory.addr_start != LOW_MEM) {
        printk("[%s:%d] no valid physics memory.\r\n", __FILE__, __LINE__);
        return;
    }

    g_physics_memory.pages_total = g_physics_memory.valid_mem_size >> 12;
    g_physics_memory.pages_used = 0;
    g_physics_memory.pages_free = g_physics_memory.pages_total - g_physics_memory.pages_used;
}

//位图初始化
void memory_map_init(void)
{
    if (g_physics_memory.addr_start != LOW_MEM) {
        printk("[%s:%d] no valid physics memory.\r\n", __FILE__, __LINE__);
        return;
    }

    g_physics_memory_map.addr_base = (uint)LOW_MEM;
    //位图放在低端1MB内存内
    g_physics_memory_map.map = (uchar *)0x10000;
    g_physics_memory_map.pages_total = g_physics_memory.pages_total;
    //这里可以看到，位图是用1B来管理1MB以上的1页物理内存，太浪费了，应该用1bit管理1页
    memset(g_physics_memory_map.map, 0, g_physics_memory_map.pages_total);
    g_physics_memory_map.bitmap_item_used = 0;
}

/* 
 * 分配一个物理页, 成功返回物理页的起始地，失败返回NULL
 * 该物理内存的分配没有设计可言，就是按顺序分配，一次分配一个页
 */
void *get_free_page(void)
{
    bool find = false;
    int i;

    for (i = g_physics_memory_map.bitmap_item_used; i < g_physics_memory.pages_total; ++i) {
        /* map[i]中的值为0，代表其所管理的那一页物理内存未使用 */
        if (g_physics_memory_map.map[i] == 0) {
            find = true;
            break;
        }
    }

    if (!find) {
        printk("memory used up!\r\n");
        return NULL;
    }

    g_physics_memory_map.map[i] = 1;
    g_physics_memory_map.bitmap_item_used++;

    void *ret = (void *)(g_physics_memory_map.addr_base + (i << 12)); //以页为单位分配
    printk("[%s]return :0x%04X, used: %d pages\r\n", __FUNCTION__, ret, g_physics_memory_map.bitmap_item_used);
    return ret;
}

/*
 * 释放一个物理页，p为待释放物理页的起始地址
*/
void free_page(void *p) 
{
    if ((uint)p < g_physics_memory.addr_start || (uint)p > g_physics_memory.addr_end) {
        printk("invalid physics address.\r\n");
        return;
    }

    int index = (int)(p - g_physics_memory_map.addr_base) >> 12;
    g_physics_memory_map.map[index] = 0;
    g_physics_memory_map.bitmap_item_used--;
    printk("[%s]return: 0x%04X, used: %d pages\r\n", __FUNCTION__, p, g_physics_memory_map.bitmap_item_used);
}

/*
 * 下面代码是开启分页机制
 * 一个pdt 4k
 * 4g内存需要 0x1000 * 0x1000 + 0x1000 大小的页表来完整映射内存
*/
#define PDT_START_ADDR      0x20000     //页表从该地址开始存放
#define VIRTUAL_MEM_START   0x200000    //线性地址从2MB开始存放

void *virtual_memory_init(void)
{
    int *pdt = (int *)PDT_START_ADDR;
    int *pde_ptr;

    memset(pdt, 0, PAGE_SIZE);
    for (int i = 0; i < 4; ++i) {
        //每一项pde都对应一个页表+低12位的权限值
        int pde = (int)PDT_START_ADDR + ((i + 1) * 0x1000);
        pde |= 0x07;
        pdt[i] = pde;
        pde_ptr = (int *)(pde & 0xFFFFFFF8); //去除权限值
        if (i == 0) {
            //第一块映射区给内核用
            for (int j = 0; j < 1024; ++j) {
                int *item = &pde_ptr[j];
                int virtual_addr = j * 0x1000;
                *item = virtual_addr | 0x07;
            }
        } else {
           for (int j = 0; j < 1024; ++j) {
            /*
               int* item = &pde_ptr[j];
               int virtual_addr = j * 0x1000;
               virtual_addr = virtual_addr + i * 0x400 * 0x1000;
               *item = 0b00000000000000000000000000000111 | virtual_addr;
            */
           }
        }
    }

    BOCHS_DEBUG_MAGIC
    set_cr3((uint)pdt);
    enable_page();
    BOCHS_DEBUG_MAGIC
    printk("pdt addr: 0x%08X\r\n", pdt);
    return pdt;
}
