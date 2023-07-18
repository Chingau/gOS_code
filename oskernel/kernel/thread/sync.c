#include "sync.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"

/* 初始化信号量 */
void sema_init(semaphore_t *psema, uint8_t value)
{
    psema->value = value;           //为信号量赋初值
    list_init(&psema->waiters);     //初始化信号量的等待队列
}

/* 信号量 down 操作*/
void sema_down(semaphore_t *psema)
{
    /*关中断保证原子操作*/
    INTR_STATUS_T old_state = intr_disable();

    while (psema->value == 0) {
        /* 当前线程不应该已在信号量的waiters队列中 */
        if (elem_find(&psema->waiters, &running_thread()->general_tag)) {
            PANIC("sema_down: thread blocked has been in waiters_list.\n");
        }

        //若信号量的值等于0，则当前线程把自己加入该锁的等待队列
        list_append(&psema->waiters, &running_thread()->general_tag);
        thread_block(TASK_BLOCKED); //阻塞线程，直到被唤醒
    }

    /* 若value为1或被唤醒后，会执行下面代码，也就是获得了锁 */
    psema->value--;
    ASSERT(psema->value == 0);
    intr_set_status(old_state);
}

/* 信号量 up 操作 */
void sema_up(semaphore_t *psema)
{
    /* 关中断，保证原子操作 */
    INTR_STATUS_T old_state = intr_disable();
    ASSERT(psema->value == 0);
    if (!list_empty(&psema->waiters)) {
        struct task_struct *thread_blocked = elem2entry(struct task_struct, \
            general_tag, list_pop(&psema->waiters));
        thread_unblock(thread_blocked); //唤醒阻塞在信号量上的线程
    }
    psema->value++;
    ASSERT(psema->value == 1);
    intr_set_status(old_state);
}

/* 初始化锁plock */
void lock_init(lock_t *plock)
{
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 1);    //信号量为1，二元信号量
}

/* 获取锁 plock */
void lock_acquire(lock_t *plock)
{
    /* 排除曾经自己已经持有锁但还未将其释放的情况 */
    if (plock->holder != running_thread()) {
        sema_down(&plock->semaphore);   //对信号量P操作，原子操作
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
    } else {
        plock->holder_repeat_nr++;
    }
}

/* 释放锁 plock */
void lock_release(lock_t *plock)
{
    void *pholder = NULL;

    ASSERT(plock->holder == running_thread());
    if (plock->holder_repeat_nr > 1) {
        plock->holder_repeat_nr--;
        return;
    }
    ASSERT(plock->holder_repeat_nr == 1);
    __asm__("mov [%%eax], %1;" : : "a"(&plock->holder), "b"(pholder));  //下面那条语句CPU没有执行，只能先用这个内联汇编代替一下
    //plock->holder == NULL;          //把锁的持有者置空放在V操作之前   //该语句CPU没有执行，不知道为什么，导致出错   ？？？？
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);     //信号量的V操作，原子操作
}
