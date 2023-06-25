/*
 * Create by gaoxu on 2023.06.11
 * */
#include "kernel.h"
#include "mm.h"
#include "bitmap.h"
#include "types.h"

#define ARDS_ADDR   0x1100
#define LOW_MEM     0x100000    //1M以下的物理内在给内核用

#define ZONE_VAILD      1       //ards可用内存区域
#define ZONE_RESERVED   2       //ards不可用内存区域

static uint32_t mem_bytes_total = 0;   //物理内存大小

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

        if (temp->base_addr_low == LOW_MEM) {
            mem_bytes_total = temp->length_low;
            //printk("mem_bytes_total: 0x%08x\r\n", mem_bytes_total);
        }
    }
}

/***************位图地址***************
 * 因为0xc009f000是内核主栈程栈顶，0xc0009e000是内核主线程的pcb
 * 一个页框大小的位图可表示128MB内存，位图位置安排在地址0xc009a000，
 * 这样本系统最大支持4个页框的位图，即512MB
 * */
#define MEM_BITMAP_BASE 0xc009a000

/* 0xc0100000是内核从虚拟地址3G起
 * 0x100000意指跨过低端1MB内存，使虚拟地址在逻辑上连续
 * */
#define K_HEAP_START    0xc0100000

/*
 * 内存池结构，生成两个实例，用于管理内核内存池和用户内存池
 * */
struct pool {
    bitmap_t pool_bitmap;
    uint32_t phy_addr_start;
    uint32_t pool_size;
};
struct pool kernel_pool, user_pool; //生成内核内存池和用户内存池
virtual_addr_t kernel_vaddr; //此结构用来给内核分配虚拟地址

/*
 * 初始化内存池
 * */
static void mem_pool_init(uint32_t all_mem)
{
    printk("mem_pool_init start.\r\n");
    /*
     * 页表大小 = 1页的页目录表 + 第0和第768个页目录项指向同一个页表 + 第769～1022个页目录项共指向254个页表，共256个页框
     * */
    uint32_t page_table_size = PAGE_SIZE * 256;
    uint32_t used_mem = page_table_size + 0x100000; //0x100000为低端1MB内存
    uint32_t free_mem = all_mem - used_mem;
    uint16_t all_free_pages = free_mem / PAGE_SIZE; //不考虑不足1页的内存
    uint16_t kernel_free_pages = all_free_pages / 2;
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;
    /* 位图的一位表示一页，以字节为单位 */
    uint32_t kbm_length = kernel_free_pages / 8;
    uint32_t ubm_length = user_free_pages / 8;
    /* 内核与用户内存池的起始地址 */
    uint32_t kp_start = used_mem;
    uint32_t up_start = kp_start + kernel_free_pages * PAGE_SIZE;

    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_pages * PAGE_SIZE;
    user_pool.pool_size = user_free_pages * PAGE_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

    kernel_pool.pool_bitmap.bits = (void *)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length);

    printk("kernel_pool_bitmap_start: 0x%08x\r\n", (int)kernel_pool.pool_bitmap.bits);
    printk("kernel_pool_phy_addr_start: 0x%08x\r\n", kernel_pool.phy_addr_start);
    printk("user_pool_bitmap_start: 0x%08x\r\n", (int)user_pool.pool_bitmap.bits);
    printk("user_pool_phy_addr_start: 0x%08x\r\n", user_pool.phy_addr_start);

    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    /*
     * 下面初始化内核虚拟地址的位图，按实际物理内存大小生成数组
     * 用于维护内核堆的虚拟地址，所以要和内核内存池大小一致
     * */
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
    /* 位图的数组指向一块未使用的内存，目前定位在内核内存池和用户内存池之外 */
    kernel_vaddr.vaddr_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    printk("mem_pool_init done.\r\n");
}

void mem_init(void)
{
    printk("mem_init start.\r\n");
    mem_pool_init(mem_bytes_total);
    printk("mem_init done.\r\n");
}
