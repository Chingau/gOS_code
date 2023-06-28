/*
 * Create by gaoxu on 2023.06.11
 * */
#ifndef __GOS_OSKERNEL_MM_H__
#define __GOS_OSKERNEL_MM_H__
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

typedef struct {
    uint addr_start;        //物理内存起始地址
    uint addr_end;          //物理内存结束地址
    uint valid_mem_size;    //物理内存大小
    uint pages_total;       //物理内存共多少pages
    uint pages_free;        //物理内存空闲多少pages
    uint pages_used;        //物理内存用了多少pages
} physics_memory_inof_t;

typedef struct {
    uint addr_base;         //被管理的物理内存起始地址
    uint pages_total;       //共有多少空闲物理页
    uint bitmap_item_used;  //这里用1B映射1页
    uchar *map;             //位图的起始地址
} physics_memory_map_t;

void check_memory(void);
void memory_map_init(void);
void *get_free_page(void);
void free_page(void *p) ;

#endif


