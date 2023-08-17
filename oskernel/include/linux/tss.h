#ifndef __GOS_OSKERNEL_TSS_H__
#define __GOS_OSKERNEL_TSS_H__
#include "thread.h"
#include "types.h"

void update_tss_esp(struct task_struct *pthread);
void tss_init(void);

#endif
