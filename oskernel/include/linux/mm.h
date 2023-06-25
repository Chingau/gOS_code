/*
 * Create by gaoxu on 2023.06.11
 * */
#ifndef __GOS_OSKERNEL_MM_H__
#define __GOS_OSKERNEL_MM_H__
#include "bitmap.h"
#include "types.h"

#define PAGE_SIZE   4096

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

extern struct pool kernel_pool, user_pool;

void mem_init(void);
void check_memory(void);

#endif


