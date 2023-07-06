#ifndef __GOX_OSKERNEL_DEBUG_H_
#define __GOX_OSKERNEL_DEBUG_H_

void panic_spin(char *filename, int line, const char *func, const char *condition);

#define PANIC(...)  panic_spin(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#ifdef NDEBUG
    #define ASSERT(condition) ((void )0)
#else
    #define ASSERT(condition)   if (condition) {} \
                                else { \
                                    PANIC(#condition);\
                                }
#endif /* NDEBUG */

#endif
