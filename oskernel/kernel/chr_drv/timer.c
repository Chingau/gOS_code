/*
 * Created by gaoxu on 2023.06.11
 * */
#include "kernel.h"
#include "io.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"
#include "timer.h"
#include "types.h"
#include "global.h"

#define PIT_CHAN0_REG 0X40
#define PIT_CTRL_REG 0X43

#define OSCILLATOR 1193180
#define CLOCK_COUNTER (OSCILLATOR / HZ)

#define IRQ0_FREQUENCY  100
#define mil_seconds_per_intr    (1000 / IRQ0_FREQUENCY)     //每次中断时经历的ms数

unsigned int cpu_tickes = 0;    //从内核第一次处理时间中断后开始至今的滴答数

/* sleep_ticks是时钟中断发生的中断间隔数 */
static void ticks_to_sleep(uint32_t sleep_ticks)
{
    uint32_t start_tick = cpu_tickes;

    /* 若间隔的ticks数不够便让出cpu */
    while (cpu_tickes - start_tick < sleep_ticks) {
        thread_yield();
    }
}

/* 以ms为单位的sleep */
void mtime_sleep(uint32_t m_seconds)
{
    //把入参ms转化为时钟中断发生的"间隔cpu_tickes数"
    uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
    ASSERT(sleep_ticks > 0);
    ticks_to_sleep(sleep_ticks);
}

static void intr_timer_handler(void)
{
    struct task_struct *cur_thread = running_thread();
    ASSERT(cur_thread->stack_magic == 0x20000324);  //检查栈是否溢出
    cur_thread->elapsed_ticks++;        //记录此线程占用CPU的时间
    cpu_tickes++;

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
