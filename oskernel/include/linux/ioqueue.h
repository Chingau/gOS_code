#ifndef __GOS_IOQUEUE_H_
#define __GOS_IOQUEUE_H_
#include "sync.h"
#include "types.h"

#define IOQUE_BUFSIZE 64

/* 环形队列 */
typedef struct {
    lock_t lock;
    //生产者，缓冲区不满时就继续往里面放数据，否则就睡眠，此项记录哪个生产者在此缓冲区上睡眠
    struct task_struct *producer;
    //消费者，缓冲区不空时就继续从里面拿数据，否则就睡眠，此项记录哪个消费者在此缓冲区上睡眠
    struct task_struct *consumer;
    char buf[IOQUE_BUFSIZE];  //缓冲区大小
    int32_t head;       //队首，数据往队首处写入
    int32_t tail;       //队尾，数据从队尾处读出
} ioqueue_t;

void ioqueue_init(ioqueue_t *ioq);
bool ioq_full(ioqueue_t *ioq);
bool ioq_empty(ioqueue_t *ioq);
char ioq_getchar(ioqueue_t *ioq);
void ioq_putchar(ioqueue_t *ioq, char byte);

#endif
