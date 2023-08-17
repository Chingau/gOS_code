/*
 * Create by gaoxu on 2023.06.11
 * */
#ifndef __GOS_OSKERNEL_MM_H__
#define __GOS_OSKERNEL_MM_H__
#include "bitmap.h"
#include "types.h"

typedef struct {
    unsigned int base_addr_low;
    unsigned int base_addr_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type;
} check_memory_item_t;

typedef struct {
    unsigned short times;
    check_memory_item_t *data;
} check_memory_info_t;

/* 虚拟地址池，用于虚拟地址管理 */
typedef struct {
    bitmap_t vaddr_bitmap;  //虚拟地址用到的位图结构
    uint32_t vaddr_start;   //虚拟地址起始地址
} virtual_addr_t;

/* 内存池标记，用于判断用哪个内存池 */
enum pool_flags {
    PF_KERNEL = 1,  //内核内存池
    PF_USER = 2     //用户内存池
};

#define PG_P_1  1       //页表项或页目录项存在属性位
#define PG_P_0  0       //页表项或页目录项存在属性位
#define PG_RW_R 0       // R/W属性位值，读/执行
#define PG_RW_W 2       // R/W属性位值，读/写/执行
#define PG_US_S 0       // U/S属性位值，系统级
#define PG_US_U 4       // U/S属性位值，用户级

extern struct pool kernel_pool, user_pool;

void mem_init(void);
void check_memory(void);
void *get_kernel_pages(uint32_t pg_cnt);
void *get_user_pages(uint32_t pg_cnt);
void *get_a_page(enum pool_flags pf, uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);

#endif


