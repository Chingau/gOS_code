#ifndef __GOS_OSKERNEL_KEYBOARD_H_
#define __GOS_OSKERNEL_KEYBOARD_H_
#include "ioqueue.h"

void keyboard_init(void);

extern ioqueue_t kbd_buf;

#endif
