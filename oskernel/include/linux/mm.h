/*
 * Create by gaoxu on 2023.06.11
 * */
#ifndef __GOS_OSKERNEL_MM_H__
#define __GOS_OSKERNEL_MM_H__
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

void check_memory(void);

#endif


