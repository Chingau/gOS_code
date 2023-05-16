/*
 * create by gaoxu on 2023.05.10
 * */

#ifndef __GOS_OSKERNEL_TTY_H__
#define __GOS_OSKERNEL_TTY_H__
#include "types.h"

void console_init(void);
void console_write(char *buf, u32 count);

#endif
