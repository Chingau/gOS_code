#ifndef __GOX_OSKERNEL_INTERRUPT_H
#define __GOX_OSKERNEL_INTERRUPT_H
#include "types.h"

/* 中断状态 */
typedef enum {
    INTR_OFF,
    INTR_ON
} INTR_STATUS_T;

void idt_init(void);
void register_handler(uint8_t vector_no, void *function);

INTR_STATUS_T intr_get_status(void);
INTR_STATUS_T intr_set_status(INTR_STATUS_T status);
INTR_STATUS_T intr_enable(void);
INTR_STATUS_T intr_disable(void);

#endif
