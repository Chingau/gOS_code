#ifndef __GOS_THREAD_SYNC_H_
#define __GOS_THREAD_SYNC_H_
#include "types.h"
#include "list.h"

/* 信号量结构 */
typedef struct {
    uint8_t value;
    struct list waiters;
} semaphore_t;

/* 锁结构 */
typedef struct {
    struct task_struct *holder;     //锁的持有者
    semaphore_t semaphore;          //用二元信号量实现锁
    uint32_t holder_repeat_nr;      //锁的持有者重复申请锁的次数
} lock_t;

void sema_init(semaphore_t *psema, uint8_t value);
void sema_down(semaphore_t *psema);
void sema_up(semaphore_t *psema);
void lock_init(lock_t *plock);
void lock_acquire(lock_t *plock);
void lock_release(lock_t *plock);

#endif
