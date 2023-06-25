/*
 * Create by gaoxu on 2023.06.25
 * */

#ifndef __GOS_OSKERNEL_BITMAP_H__
#define __GOS_OSKERNEL_BITMAP_H__
#include "types.h"

#define BITMAP_MASK 1
typedef struct {
    uint32_t btmp_bytes_len;
    uint8_t *bits;
} bitmap_t;

void bitmap_init(bitmap_t *btmp);
bool bitmap_scan_test(bitmap_t *btmp, uint32_t bit_idx);
int bitmap_scan(bitmap_t *btmp, uint32_t cnt);
void bitmap_set(bitmap_t *btmp, uint32_t bit_idx, char value);

#endif
