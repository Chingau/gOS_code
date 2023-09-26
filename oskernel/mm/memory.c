/*
 * Create by gaoxu on 2023.06.11
 * */
#include "kernel.h"
#include "mm.h"
#include "bitmap.h"
#include "types.h"
#include "string.h"
#include "debug.h"
#include "sync.h"
#include "global.h"
#include "interrupt.h"

#define PAGE_SIZE   4096
#define ARDS_ADDR   0x1100
#define LOW_MEM     0x100000    //1M以下的物理内在给内核用

#define ZONE_VAILD      1       //ards可用内存区域
#define ZONE_RESERVED   2       //ards不可用内存区域

/* 内存仓库 */
typedef struct {
    mem_block_desc_t *desc;     //此arena关联的mem_block_desc
    uint32_t cnt;               //当large=true时，cnt表示页框数；当large=false时，cnt表示空闲mem_block数量
    bool large;
} arena_t;
mem_block_desc_t k_block_descs[MEM_DESC_CNT];   //内核内存块描述符数组

static uint32_t mem_bytes_total = 0;   //物理内存大小

void check_memory(void)
{
    check_memory_info_t *p = (check_memory_info_t *)ARDS_ADDR;
    check_memory_item_t *ards = (check_memory_item_t *)(ARDS_ADDR + 2);
    unsigned short times = p->times;
    check_memory_item_t *temp;

    print_unlock("BaseAddrHigh BaseAddrLow LengthHigh  LengthLow   Type\n");
    for (int i = 0; i < times; ++i) {
        temp = (ards + i);
        print_unlock("0x%08x,  0x%08x, 0x%08x, 0x%08x, %d\n",
               temp->base_addr_high, temp->base_addr_low, temp->length_high, temp->length_low, temp->type);

        if (temp->base_addr_low == LOW_MEM) {
            mem_bytes_total = temp->length_low + temp->base_addr_low;
            //print_unlock("mem_bytes_total: 0x%08x\r\n", mem_bytes_total);
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
    lock_t lock;    //用户申请内存时的互斥
};
struct pool kernel_pool, user_pool; //生成内核内存池和用户内存池
virtual_addr_t kernel_vaddr; //此结构用来给内核分配虚拟地址

/*
 * 初始化内存池
 * */
static void mem_pool_init(uint32_t all_mem)
{
    print_unlock("mem_pool_init start.\r\n");
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

    print_unlock("kernel_pool_bitmap_start: 0x%08x\r\n", (int)kernel_pool.pool_bitmap.bits);
    print_unlock("kernel_pool_phy_addr_start: 0x%08x\r\n", kernel_pool.phy_addr_start);
    print_unlock("user_pool_bitmap_start: 0x%08x\r\n", (int)user_pool.pool_bitmap.bits);
    print_unlock("user_pool_phy_addr_start: 0x%08x\r\n", user_pool.phy_addr_start);

    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

    /*
     * 下面初始化内核虚拟地址的位图，按实际物理内存大小生成数组
     * 用于维护内核堆的虚拟地址，所以要和内核内存池大小一致
     * */
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
    /* 位图的数组指向一块未使用的内存，目前定位在内核内存池和用户内存池之外 */
    kernel_vaddr.vaddr_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    print_unlock("mem_pool_init done.\r\n");
}

/* 为malloc做准备 */
void block_desc_init(mem_block_desc_t *desc_array)
{
    uint16_t desc_idx, block_size = 16;
    
    /* 初始化每个mem_block_desc描述符 */
    for (desc_idx = 0; desc_idx < MEM_DESC_CNT; ++desc_idx) {
        desc_array[desc_idx].block_size = block_size;
        /* 初始化arena中的内存块数量 */
        desc_array[desc_idx].blocks_per_arena = (PAGE_SIZE - sizeof(arena_t)) / block_size;
        list_init(&desc_array[desc_idx].free_list);
        block_size *= 2;        //更新为下一个规格内存块
    }
}

void mem_init(void)
{
    print_unlock("mem_init start.\r\n");
    mem_pool_init(mem_bytes_total);
    block_desc_init(k_block_descs);
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
        struct task_struct *curr = running_thread();

        bit_idx_start = bitmap_scan(&curr->userprog_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;   
        }
        while (cnt < pg_cnt) {
            bitmap_set(&curr->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = curr->userprog_vaddr.vaddr_start + bit_idx_start * PAGE_SIZE;
        /* (0xc0000000 - PG_SIZE)作为用户3级栈已经在start_process被分配 */
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }
    return (void *)vaddr_start;
}

/* 在虚拟地址池中释放以_vaddr起始的连续pg_cnt个虚拟页地址 */
static void vaddr_remove(enum pool_flags pf, void *_vaddr, uint32_t pg_cnt)
{
    uint32_t bit_idx_start = 0;
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t cnt = 0;

    if (pf == PF_KERNEL) {
        bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PAGE_SIZE;
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    } else {
        struct task_struct *curr_thread = running_thread();
        bit_idx_start = (vaddr - curr_thread->userprog_vaddr.vaddr_start) / PAGE_SIZE;
        while (cnt < pg_cnt) {
            bitmap_set(&curr_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }
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

/* 将物理地址pg_phy_addr回收到物理内存池 */
void pfree(uint32_t pg_phy_addr)
{
    struct pool *mem_pool;
    uint32_t bit_idx = 0;

    if (pg_phy_addr >= user_pool.phy_addr_start) {
        //用户物理内存池
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PAGE_SIZE;
    } else {
        //内核物理内存池
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PAGE_SIZE;
    }
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
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

/* 去掉页表中虚拟地址vaddr的映射，只去掉vaddr对应的pte */
static void page_table_pte_remove(uint32_t vaddr)
{
    uint32_t *pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;    //将页表项pte的P位置0
    __asm__ volatile("invlpg %0"::"m"(vaddr):"memory"); //更新tlb
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

/* 释放以虚拟地址vaddr为起始的cnt个物理页框 */
void mfree_page(enum pool_flags pf, void *_vaddr, uint32_t pg_cnt)
{
    uint32_t pg_phy_addr;
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t page_cnt = 0;

    ASSERT(pg_cnt >= 1 && vaddr % PAGE_SIZE == 0);
    pg_phy_addr = addr_v2p(vaddr);  //获取虚拟地址vaddr对应的物理地址

    /* 确保待释放的物理内存在低端1MB+1KB大小的页目录+1KB大小的页表地址范围外 */
    ASSERT((pg_phy_addr % PAGE_SIZE) == 0 && pg_phy_addr >= 0x102000);
    if (pg_phy_addr >= user_pool.phy_addr_start) {
        vaddr -= PAGE_SIZE;
        while (page_cnt < pg_cnt) {
            vaddr += PAGE_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            ASSERT((pg_phy_addr % PAGE_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);
            pfree(pg_phy_addr);     //先将对应的物理页框归还到内存池
            page_table_pte_remove(vaddr);   //再从页表中清除此虚拟地址所在的页表项pte
            page_cnt++;
        }
        vaddr_remove(pf, _vaddr, pg_cnt);   //清空虚拟地址的位图中相应位
    } else {
        vaddr -= PAGE_SIZE;
        while (page_cnt < pg_cnt) {
            vaddr += PAGE_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            ASSERT((pg_phy_addr % PAGE_SIZE) == 0 && \
                    pg_phy_addr >= kernel_pool.phy_addr_start && \
                    pg_phy_addr < user_pool.phy_addr_start);
            pfree(pg_phy_addr);
            page_table_pte_remove(vaddr);
            page_cnt++;
        }
        vaddr_remove(pf, _vaddr, pg_cnt);
    }
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

/*
 * 在用户空间中申请4KB内存，并返回其虚拟地址
*/
void *get_user_pages(uint32_t pg_cnt)
{
    lock_acquire(&user_pool.lock);
    void *vaddr = malloc_page(PF_USER, pg_cnt);
    memset(vaddr, 0, pg_cnt * PAGE_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

/*
 * 将地址vaddr与pf池中的物理地址关联，仅支持一页空间分配
*/
void *get_a_page(enum pool_flags pf, uint32_t vaddr)
{
    struct pool *mem_pool = (pf & PF_KERNEL) ? &kernel_pool : &user_pool;

    lock_acquire(&mem_pool->lock);
    //先将虚拟地址对应的位图置1
    struct task_struct *curr = running_thread();
    int32_t bit_idx = -1;

    //若当前是用户进程申请用户内存，就修改用户进程自己的虚拟地址位图
    if (curr->pgdir != NULL && pf == PF_USER) {
        bit_idx = (vaddr - curr->userprog_vaddr.vaddr_start) / PAGE_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&curr->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
    } else if (curr->pgdir == NULL && pf == PF_KERNEL) {
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PAGE_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
    } else {
        PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page.");
    }

    void *page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) {
        return NULL;
    }
    page_table_add((void *)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void *)vaddr;
}

/*
 * 得到虚拟地址映射到的物理地址
*/
uint32_t addr_v2p(uint32_t vaddr)
{
    uint32_t *pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

/* 返回arena中第idx个内存块的地址 */
static mem_block_t *arena2block(arena_t *a, uint32_t idx)
{
    return (mem_block_t *)((uint32_t)a + sizeof(arena_t) + idx * a->desc->block_size);
}

/* 返回内存块b所在的arena地址 */
static arena_t *block2arena(mem_block_t *b)
{
    return (arena_t *)((uint32_t)b & 0xfffff000);
}

/* 在堆中申请size字节内存 */
void *sys_malloc(uint32_t size)
{
    enum pool_flags PF;
    struct pool *mem_pool;
    uint32_t pool_size;
    mem_block_desc_t *descs;
    struct task_struct *curr_thread = running_thread();

    /* 判断用哪个内存池 */
    if (curr_thread->pgdir == NULL) {
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;
        mem_pool = &kernel_pool;
        descs = k_block_descs;
    } else {
        PF = PF_USER;
        pool_size = user_pool.pool_size;
        mem_pool = &user_pool;
        descs = curr_thread->u_block_desc;
    }

    /* 合法性判断 */
    if (!(size > 0 && size < pool_size)) {
        return NULL;
    }

    arena_t *a;
    mem_block_t *b;

    lock_acquire(&mem_pool->lock);
    if (size > 1024) {  //超过最大内存块1024，就分配页框
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(arena_t), PAGE_SIZE);    //向上取整需要的页框数

        a = malloc_page(PF, page_cnt);
        if (a != NULL) {
            memset(a, 0, page_cnt * PAGE_SIZE);
            a->desc = NULL;
            a->cnt = page_cnt;
            a->large = true;
            lock_release(&mem_pool->lock);
            return (void *)(a + 1);     //跨过arena大小，把剩下的内存返回
        } else {
            lock_release(&mem_pool->lock);
            return NULL;
        }
    } else {    //若申请的内存小于等于1024，可在各种规格的mem_block_desc中去适配
        uint8_t desc_idx;

        /* 从内存块描述符中匹配合适的内存块规格 */
        for (desc_idx = 0; desc_idx < MEM_DESC_CNT; ++desc_idx) {
            if (size <= descs[desc_idx].block_size) {
                break;  //从小往大，找到后退出
            }
        }

        /* 若mem_block_desc的free_list中已经没有可用的mem_block，就创建新的arena提供mem_block */
        if (list_empty(&descs[desc_idx].free_list)) {
            a = malloc_page(PF, 1);     //分配一个页框做为arena
            if (a == NULL) {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a, 0, PAGE_SIZE);

            /* 对于分配的小块内存，将desc置为相应内存块描述符，cnt置为此arena可用的内存块数，large置为false */
            a->desc = &descs[desc_idx];
            a->large = false;
            a->cnt = descs[desc_idx].blocks_per_arena;

            uint32_t block_idx;

            INTR_STATUS_T old_status = intr_disable();
            /* 开始将arena拆分成内存块，并添加到内存块描述符的free_list中 */
            for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; ++block_idx) {
                b = arena2block(a, block_idx);
                ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }
            intr_set_status(old_status);
        }

        /* 开始分配内存块 */
        b = elem2entry(mem_block_t, free_elem, list_pop(&(descs[desc_idx].free_list)));
        memset(b, 0, descs[desc_idx].block_size);
        a = block2arena(b);     //获取内存块b所在的arena
        a->cnt--;               //将此arena中的空闲内存块数减1
        lock_release(&mem_pool->lock);
        return (void *)b;
    }
}

/* 回收内存ptr */
void sys_free(void *ptr)
{
    ASSERT(ptr != NULL);
    if (ptr == NULL)
        return;

    enum pool_flags PF;
    struct pool *mem_pool;

    if (running_thread()->pgdir == NULL) {
        ASSERT((uint32_t)ptr >= K_HEAP_START);
        PF = PF_KERNEL;
        mem_pool = &kernel_pool;
    } else {
        PF = PF_USER;
        mem_pool = &user_pool;
    }

    lock_acquire(&mem_pool->lock);
    mem_block_t *b = ptr;
    arena_t *a = block2arena(b);
    ASSERT(a->large == 0 || a->large == 1);
    if (a->desc == NULL && a->large == true) {  //大于1024的内存
        mfree_page(PF, a, a->cnt);
    } else {
        list_append(&a->desc->free_list, &b->free_elem);    //先将内存块回收到free_list中
        /* 在判断此arena中的内存块是否都是空闲，如果是就释放arena */
        if (++a->cnt == a->desc->blocks_per_arena) {
            uint32_t block_idx;
            for (block_idx = 0; block_idx < a->desc->blocks_per_arena; ++block_idx) {
                mem_block_t *b = arena2block(a, block_idx);
                ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                list_remove(&b->free_elem);
            }
            mfree_page(PF, a, 1);
        }
    }
    lock_release(&mem_pool->lock);
}

/* 安装1页大小的vaddr，专门针对fork时虚拟地址位图无需操作的情况 */
void *get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr)
{
    struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    void *page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) {
        lock_release(&mem_pool->lock);
        return NULL;
    }
    page_table_add((void *)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void *)vaddr;
}

/* 根据物理页框地址pg_phy_addr在相应的内存池的位图清0，不改动页表 */
void free_a_phy_page(uint32_t pg_phy_addr)
{
    struct pool *mem_pool;
    uint32_t bit_idx = 0;

    if (pg_phy_addr >= user_pool.phy_addr_start) {
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    } else {
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;        
    }
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}