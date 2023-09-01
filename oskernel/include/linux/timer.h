#ifndef __GOS_OSKERNEL_TIMER_H_
#define __GOS_OSKERNEL_TIMER_H_

#define HZ 100  //每秒时钟中断的次数，中断号0x21

void timer_init(void);
void mtime_sleep(uint32_t m_seconds);

#endif
