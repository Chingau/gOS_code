#ifndef __GOS_OSKERNEL_STDARG_H__
#define __GOS_OSKERNEL_STDARG_H__

typedef char * va_list;
#define va_start(p, arg) (p = (va_list)&arg + sizeof(arg))
#define va_arg(p, t) (*(t *)((p += sizeof(t)) - sizeof(t)))
#define va_end(p) (p = (va_list)0)

#endif
