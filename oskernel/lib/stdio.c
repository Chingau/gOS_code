#include "kernel.h"
#include "stdio.h"
#include "syscall.h"

uint32_t printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char buf[1024] = {0};
    vsprintf(buf, format, args);
    va_end(args);
    return write(buf);
}

int sprintf(char *str, const char *format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = vsprintf(str, format, args);
    va_end(args);
    return ret;
}