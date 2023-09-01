#ifndef __GOS_OSKERNEL_STDIO_H
#define __GOS_OSKERNEL_STDIO_H
#include "types.h"

uint32_t printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);

#endif
