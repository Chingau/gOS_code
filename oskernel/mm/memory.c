/*
 * Create by gaoxu on 2023.06.11
 * */
#include "kernel.h"
#include "mm.h"
#include "bitmap.h"
#include "types.h"
#include "string.h"
#include "debug.h"

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
            mem_bytes_total = temp->length_low + temp->base_addr_low;
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

#define PDE_IDX(addr)   ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr)   ((addr & 0x003ff000) >> 12)

/*
 * 在pf表示的虚拟内存池中申请pg_cnt个虚拟页
 * 成功则返回虚拟页的起始地址，失败则返回NULL
 * */
static void *vaddr_get(enum pool_flags pf, uint32_t pg_cnt)
{
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;

    if (pf == PF_KERNEL) {
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        }
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PAGE_SIZE;
    } else {
        //todo:用户内存池，将来实现用户进程再补充
    }
    return (void *)vaddr_start;
}

/*
 * 由虚拟地址vaddr得到指向pte的虚拟地址
 * */
uint32_t *pte_ptr(uint32_t vaddr)
{
    /*
     * 先访问到页表自己 +
     * 再用页目录项pde(页目录内页表的索引)作为pte的索引访问到页表 +
     * 再用pte的索引作为页内偏移
     * */
    uint32_t *pte = (uint32_t *)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

/*
 * 由虚拟地址vaddr得到指向pde的虚拟地址
 * */
uint32_t *pde_ptr(uint32_t vaddr)
{
    /* 0xfffff 用来访问到页表本身所在的地址 */
    uint32_t *pde = (uint32_t *)(0xfffff000 + PDE_IDX(vaddr) * 4);
    return pde;
}

/*
 * 在m_pool指向的物理内存池中分配1个物理页
 * 成功则返回页框的物理地址，失败则返回NULL
 * */
static void *palloc(struct pool *m_pool)
{
    /* 扫描或设置位图要保证原子操作 */
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1); //找到一个物理页
    if (bit_idx == -1) {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
    uint32_t page_phyaddr = (bit_idx * PAGE_SIZE) + m_pool->phy_addr_start;
    return (void *)page_phyaddr;
}

/*
 * 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射
 * */
static void page_table_add(void *_vaddr, void * _page_phyaddr)
{
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t *pde = pde_ptr(vaddr);
    uint32_t *pte = pte_ptr(vaddr);

    /* **************** 注意 ****************
     * 执行 *pte 会访问到空的 pde。所以确保 pde 创建完成后才能执行 *pte，
     * 否则会引发 page_fault。因此在 *pde 为0时，pte 只能出现在下面 else 语句块中的 *pde 后面
     * */
    /* 先在页目录内判断目录项的P位，若为1,则表示该表已存在 */
    if (*pde & 0x00000001) {
        ASSERT(!(*pte & 0x00000001));

        /* 只要是创建页表，pte就应该不存在 */
        if (!(*pte & 0x00000001)) {
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        } else {
            PANIC("pte repeat.");
        }
    } else {
        /*
         * 页目录项不存在，所以要先创建页目录再创建页表项
         * 页表中用到的页框一律从内核空间分配
         * */
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        /*
         * 分配到的物理页地址pde_phyaddr对应的物理内存清0,
         * 访问到pde对应的物理地址，用pte取高20位即可。
         * 因为pte基于该pde对应的物理地址内寻址，
         * 把低12位置0便是该pde对应的物理页的起始
         * */
        memset((void *)((int)pte & 0xfffff000), 0, PAGE_SIZE);
        ASSERT(!(*pte & 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

/*
 * 分配pg_cnt个页空间，成功则返回起始虚拟地址，失败则返回NULL
 * */
void *malloc_page(enum pool_flags pf, uint32_t pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
    /**************** malloc_page的原理是三个动作的合成：****************
     * 1.通过 vaddr_get 在虚拟内存池中申请虚拟地址
     * 2.通过 palloc 在物理内存池中申请物理页
     * 3.通过 page_table_add 将以上得到虚拟地址和物理地址在页表中完成映射
     * */
    void *vaddr_start = vaddr_get(pf, pg_cnt);
    if (vaddr_start == NULL) {
        return NULL;
    }

    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    /*
     * 因为虚拟地址是连续的，但物理地址可以不连续，所以逐个做映射
     * */
    while (cnt-- > 0) {
        void *page_phyaddr = palloc(mem_pool);
        /*
         * 失败时要将曾经已申请的虚拟地址和物理页全部回滚，在将来完成内存回收时再补充
         * */
        if (page_phyaddr == NULL) {
            return NULL;
        }
        page_table_add((void *)vaddr, page_phyaddr);
        vaddr += PAGE_SIZE;
    }
    return vaddr_start;
}

/*
 * 从内核物理内存池中申请pg_cnt页内存
 * 成功返回其虚拟地址，失败返回NULL
 * */
void *get_kernel_pages(uint32_t pg_cnt)
{
    void *vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PAGE_SIZE);
    }
    return vaddr;
}
