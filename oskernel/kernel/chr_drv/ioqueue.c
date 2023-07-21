#include "ioqueue.h"
#include "debug.h"
#include "interrupt.h"

/* 初始化io队列ioq */
void ioqueue_init(ioqueue_t *ioq)
{
    lock_init(&ioq->lock);
    ioq->producer = ioq->consumer = NULL;
    ioq->head = ioq->tail = 0;
}

/* 返回pos在缓冲区中的下一个位置值 */
static int32_t next_pos(int32_t pos)
{
    return (pos + 1) % IOQUE_BUFSIZE;
}

/* 判断队列是否已满；true已满，false未满 */
bool ioq_full(ioqueue_t *ioq)
{
    ASSERT(intr_get_status() == INTR_OFF);
    return next_pos(ioq->head) == ioq->tail;
}

/* 判断队列是否为空；true为空，flase未空 */
bool ioq_empty(ioqueue_t *ioq)
{
    ASSERT(intr_get_status() == INTR_OFF);
    return ioq->head == ioq->tail;
}

/*
* 使当前生产者或消费者在此缓冲区上等待
*/
static void ioq_wait(struct task_struct **waiter)
{
    ASSERT(*waiter == NULL && waiter != NULL);
    *waiter = running_thread();
    thread_block(TASK_BLOCKED);
}

/* 唤醒waiter */
static void wakeup(struct task_struct **waiter)
{
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);
    *waiter = NULL;
}

/* 消费者从ioq队列中获取一个字符 */
char ioq_getchar(ioqueue_t *ioq)
{
    ASSERT(intr_get_status() == INTR_OFF);
    /*
     若缓冲区(队列)为空，把消费者ioq->consumer记为当前线程自己，
     目的是将来生产者往缓冲区里装商品后，生产者知道唤醒哪个消费者，
     也就是唤醒当前线程自己
    */
    while (ioq_empty(ioq)) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->consumer);
        lock_release(&ioq->lock);
    }

    char byte = ioq->buf[ioq->tail];
    ioq->tail = next_pos(ioq->tail);

    if (ioq->producer != NULL) {
        wakeup(&ioq->producer); //唤醒生产者
    }

    return byte;
}

/* 生产者往ioq队列中写入一个字符byte */
void ioq_putchar(ioqueue_t *ioq, char byte)
{
    ASSERT(intr_get_status() == INTR_OFF);
    /*
     若缓冲区(队列)已满，把生产者ioq->producer记为自己，
     为的是当缓冲区的东西被消费者取完后让消费者知道唤醒哪个生产者，
     也就是唤醒当前线程自己
    */ 
    while (ioq_full(ioq)) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->producer);
        lock_release(&ioq->lock);
    }

    ioq->buf[ioq->head] = byte;
    ioq->head = next_pos(ioq->head);

    if (ioq->consumer != NULL) {
        wakeup(&ioq->consumer); //唤醒消费者
    }
}
