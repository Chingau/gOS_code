/*
 * Create by gaoxu on 2023.05.19
 * */
#ifndef __GOS_OSKERNEL_COMMON_H__
#define __GOS_OSKERNEL_COMMON_H__
#include "types.h"

#define DEBUG
#define BOCHS_DEBUG_MAGIC   __asm__("xchg bx, bx");//bochs调试断点
#define STI __asm__("sti")
#define CLI __asm__("cli")

typedef enum {
    rc_black = 0,
    rc_blue = 1,
    rc_green = 2,
    rc_cyan = 3,
    rc_red = 4,
    rc_magenta = 5,
    rc_brown = 6,
    rc_light_grey = 7,
    rc_dark_grey = 8,
    rc_light_blue = 9,
    rc_light_green = 10,
    rc_light_cyan = 11,
    rc_light_red = 12,
    rc_light_magenta = 13,
    rc_light_brown = 14,
    rc_white = 15
} real_color_t;

#endif
