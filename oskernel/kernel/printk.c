/*
 * create by gaoxu on 2023.05.15
 * 实现print_unlock函数
 * */
#include "tty.h"
#include "kernel.h"
#include "sync.h"

static lock_t console_lock; //控制台锁
static char buf[1024] = {0};

//初始化终端锁
void console_lock_init(void)
{
    lock_init(&console_lock);
}

//获取终端锁
static void console_acquire(void)
{
    lock_acquire(&console_lock);
}

//释放终端锁
static void console_release(void)
{
    lock_release(&console_lock);
}

//无锁版本打印
int print_unlock(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    console_write(buf, i);
    return i;    
}

//有锁版本打印
int printk(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    console_acquire();
    console_write(buf, i);
    console_release();
    return i;
}
