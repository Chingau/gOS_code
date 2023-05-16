#include "io.h"
#include "tty.h"

void kernel_main(void)
{
    console_init();

    char *s = "gaoxu";
    for (int i = 0; i < 10; ++i) {
        printk("name:%s, index:%d\n", s, i);
    }
    while (1);
}
