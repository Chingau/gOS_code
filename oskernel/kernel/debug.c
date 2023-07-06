#include "debug.h"
#include "interrupt.h"
#include "kernel.h"

void panic_spin(char *filename, int line, const char *func, const char *condition)
{
    //因为有时候会单独调用panic_spin，所以在此处关中断
    intr_disable();
    printk("---------------kernel panic---------------\r\n");
    printk("filename: %s\r\n", filename);
    printk("line: %d\r\n", line);
    printk("function: %s\r\n", func);
    printk("condition: %s\r\n", condition);
    while(1);
}
