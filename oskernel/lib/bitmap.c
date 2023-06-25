#include "bitmap.h"
#include "string.h"
#include "types.h"

/*
 * 初始化位图btmp
 * */
void bitmap_init(bitmap_t *btmp)
{
    memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

/*
 * btmp:位图结构
 * bit_idx:索引
 * 判断bit_idx位是否为1,若为1,则返回true,否则返回false
 * */
bool bitmap_scan_test(bitmap_t *btmp, uint32_t bit_idx)
{
    uint32_t byte_index = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;
    return !!(btmp->bits[byte_index] & (BITMAP_MASK << bit_odd));
}

/*
 * 在位图btmp中申请连续cnt个位，成功则返回其起始下标，失败返回-1
 * */
int bitmap_scan(bitmap_t *btmp, uint32_t cnt)
{
    uint32_t idx_byte = 0;

    while (btmp->bits[idx_byte] == 0xff && idx_byte < btmp->btmp_bytes_len)
        idx_byte++;

    if (idx_byte >= btmp->btmp_bytes_len) {
        return -1;
    }

    int idx_bit = 0;
    while ((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte])
        idx_bit++;

    int bit_idx_start = idx_byte * 8 + idx_bit; //空闲位在位图内的下标
    if (cnt == 1) {
        return bit_idx_start;
    }

    uint32_t bit_left = btmp->btmp_bytes_len * 8 - bit_idx_start; //记录还有多少位可以判断
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1;     //用于记录找到的空闲位的个数

    bit_idx_start = -1;
    while (bit_left-- > 0) {
        if (!bitmap_scan_test(btmp, next_bit)) {
            count++;
        } else {
            count = 0;
        }
        if (count == cnt) {
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}

/*
 * 将位图btmp的bit_idx位设置为value
 * */
void bitmap_set(bitmap_t *btmp, uint32_t bit_idx, char value)
{
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;
    if (!!value) {
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    } else {
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
    }
}
