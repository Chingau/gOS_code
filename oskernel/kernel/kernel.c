/*
* Create by gaoxu on 2023.06.29
*/
#include "types.h"

uint get_cr3(void)
{
    __asm__  volatile("mov eax, cr3;");
}

void set_cr3(uint v)
{
    __asm__ volatile("mov cr3, eax;" ::"a"(v));
}

void enable_page(void)
{
    __asm__ volatile("mov eax, cr0;"
                     "or eax, 0x80000000;"
                     "mov cr0, eax;");
}
