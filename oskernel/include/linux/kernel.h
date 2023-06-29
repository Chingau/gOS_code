//
// Created by ziya on 22-6-26.
//

#ifndef __GOS_OSKERNEL_KERNEL_H__
#define __GOS_OSKERNEL_KERNEL_H__
#include "stdarg.h"
#include "types.h"

int vsprintf(char *buf, const char *fmt, va_list args);
int printk(const char * fmt, ...);

uint get_cr3(void);
void set_cr3(uint v);
void enable_page(void);

#endif

