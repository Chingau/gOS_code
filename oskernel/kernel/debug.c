#include "debug.h"
#include "interrupt.h"
#include "kernel.h"

void panic_spin(char *filename, int line, const char *func, const char *condition)
{
    //因为有时候会单独调用panic_spin，所以在此处关中断
    intr_disable();
    print_unlock("---------------kernel panic---------------\r\n");
    print_unlock("filename: %s\r\n", filename);
    print_unlock("line: %d\r\n", line);
    print_unlock("function: %s\r\n", func);
    print_unlock("condition: %s\r\n", condition);
    while(1);
}
