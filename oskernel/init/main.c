#include "io.h"
#include "tty.h"

void kernel_main(void)
{
    console_clear();
    console_write("Hello world.", 12);
}
