/*
 * create by gaoxu on 2023.05.15
 * 实现printk函数
 * */
#include "tty.h"
#include "kernel.h"

static char buf[1024] = {0};

int printk(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    console_write(buf, i);
    return i;
}
