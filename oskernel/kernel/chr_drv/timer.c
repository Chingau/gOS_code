/*
 * Created by gaoxu on 2023.06.11
 * */
#include "kernel.h"
#include "io.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"
#include "timer.h"

#define PIT_CHAN0_REG 0X40
#define PIT_CTRL_REG 0X43

#define OSCILLATOR 1193180
#define CLOCK_COUNTER (OSCILLATOR / HZ)

unsigned int cpu_tickes = 0;

static void intr_timer_handler(void)
{
    struct task_struct *cur_thread = running_thread();
    ASSERT(cur_thread->stack_magic == 0x20000324);  //检查栈是否溢出
    cur_thread->elapsed_ticks++;        //记录此线程占用CPU的时间
    cpu_tickes++;       //从内核第一次处理时间中断后开始至今的滴答数

    if (cur_thread->ticks == 0) {
        schedule(); //时间片用完则开始调度新的进程上CPU
    } else {
        cur_thread->ticks--;
    }
}

void timer_init(void)
{
    outb(PIT_CTRL_REG, 0b00110100);
    outb(PIT_CHAN0_REG, CLOCK_COUNTER & 0xff);
    outb(PIT_CHAN0_REG, (CLOCK_COUNTER >> 8) & 0xff);

    register_handler(0x20, intr_timer_handler);
}
