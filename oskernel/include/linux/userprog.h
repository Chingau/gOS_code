#ifndef __GOS_OSKERNEL_USERPROG_H__
#define __GOS_OSKERNEL_USERPROG_H__
#include "thread.h"
#include "types.h"

#define USER_STACK3_VADDR   (0xc0000000 - 0x1000)
#define USER_VADDR_START    0x8048000   //用户进程的起始地址
#define DEFAULT_PRIO        10

void page_dir_activate(struct task_struct *pthread);
void process_activate(struct task_struct *pthread);
void process_execute(void *filename, char *name, uint8_t prio);
uint32_t *create_page_dir(void);
#endif
