#include "thread.h"
#include "mm.h"
#include "string.h"
#include "kernel.h"
#include "interrupt.h"
#include "debug.h"
#include "global.h"
#include "userprog.h"
#include "sync.h"

extern void switch_to(struct task_struct *curr, struct task_struct *next);

struct task_struct *main_thread;        //主线程PCB
struct task_struct *idle_thread;        //idle线程
struct list thread_ready_list;          //就绪队列
struct list thread_all_list;            //所有任务队列
static struct list_elem *thread_tag;
lock_t pid_lock;                   //分配pid锁

//函数声明
static pid_t allocate_pid(void);

/* 获取当前线程PCB指针 */
struct task_struct *running_thread(void)
{
    uint32_t esp;
    __asm__("mov %0, %%esp" : "=g"(esp));   //intel: mov eax, esp
    return (struct task_struct *)(esp & 0xFFFFF000);
}

static void kernel_thread(thread_func *function, void *arg)
{
    /* 执行function前要开中断，避免后面的时钟中断被屏蔽而无法调度其他线程 */
    intr_enable();
    function(arg);
}

void init_thread(struct task_struct* pthread, char *name, uint8_t prio)
{
    memset(pthread, 0, sizeof(*pthread));
    pthread->pid = allocate_pid();
    strcpy(pthread->name, name);
    if (pthread == main_thread) {
        pthread->status = TASK_RUNNING; //main函数就是主线程，此时它正在运行
    } else {
        pthread->status = TASK_READY;
    }

    pthread->self_kstack = (uint32_t *)((uint32_t)pthread + PG_SIZE);
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;      //只有进程才有自己的页表(后续分配)，线程没有
    pthread->stack_magic = 0x20000324;
}

/* 该函数只有在首次任务调度时才会被执行 */
void thread_create(struct task_struct* pthread, thread_func *function, void *arg)
{
    pthread->self_kstack -= sizeof(intr_stack_t);   //预留出中断栈的空间
    pthread->self_kstack -= sizeof(thread_stack_t); //预留出线程栈的空间

    thread_stack_t *kthread_stack = (thread_stack_t *)pthread->self_kstack;
    
    kthread_stack->func_arg = arg;
    kthread_stack->func = function;
    //kthread_stack->unused_retaddr  不用操作，只起到占位作用，充当假的返回地址
    kthread_stack->eip = kernel_thread;
    kthread_stack->esi = 0;
    kthread_stack->edi = 0;
    kthread_stack->ebx = 0;
    kthread_stack->ebp = 0;
}

/*
* 创建并启动一个线程
* @param name:线程名
* @param prio:线程优先级
* @param func:线程执行函数
* @param func_arg:线程执行函数入参
* @return 返回线程PCB
*/
struct task_struct* thread_start(char *name, uint8_t prio, thread_func *func, void *func_arg)
{
    // PCB都位于内核空间，包括用户进程PCB也在内核空间
    struct task_struct *thread = (struct task_struct *)get_kernel_pages(1);
    print_unlock("%s thread vaddr:0x%08x\r\n", name, thread);

    init_thread(thread, name, prio);
    thread_create(thread, func, func_arg);
    /* 确保当前创建的线程不在就绪队列和所有任务队列中 */
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    /* 将当前创建的线程添加到就绪和所有任务队列中 */
    list_append(&thread_ready_list, &thread->general_tag);
    list_append(&thread_all_list, &thread->all_list_tag);

    return thread;
}

/* 将kernel中的main函数完善为主线程 */
static void make_main_thread(void)
{
    /*
     * 因为main早已在运行，我们在setup.asm中就设置了 mov esp, 0x9f000
     * 就是为其主线程预留pcb的，因此主线程pcb地址为0x9e000，
     * 所以不需要为主线程再调用get_kernel_pages函数分配页内存了
    */
    main_thread = running_thread();
    init_thread(main_thread, "main", 10);
    
    /* main函数就是当前线程，当前线程不在就绪队列中，所以只有添加到thread_all_list队列中即可 */
    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);
}

/* 系统空闲时运行的线程 */
static void idle(void *arg UNUSED)
{
    while (1) {
        thread_block(TASK_BLOCKED);
        //执行hlt时必须要保证目前处在开中断的情况下
        __asm__ volatile("sti; hlt" ::: "memory");
    }
}

/* 实现任务调度 */
void schedule(void)
{
    ASSERT(intr_get_status() == INTR_OFF);

    //如果就绪队列中没有可运行的任务就唤醒idle
    if (list_empty(&thread_ready_list)) {
        thread_unblock(idle_thread);
    }

    struct task_struct *curr = running_thread();
    if (curr->status == TASK_RUNNING) {
        //若此线程只是CPU时间片到了，将其加入到就绪队列队尾
        ASSERT(!elem_find(&thread_ready_list, &curr->general_tag));
        list_append(&thread_ready_list, &curr->general_tag);
        curr->ticks = curr->priority;
        curr->status = TASK_READY;
    } else {
        /*
         若此线程需要某事件发生后才能继续上CPU运行，不需要将其加入队列，因为当前线程不在就绪队列中
        */
    }

    //从就绪队列首取出一个任务上CPU运行
    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct *next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;
    //激活任务页表等
    process_activate(next);
    switch_to(curr, next);
}

void thread_init(void)
{
    print_unlock("thread init...\r\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    lock_init(&pid_lock);
    make_main_thread(); //将当前main函数创建为线程
    idle_thread = thread_start("idle", 5, idle, NULL);  //创建idle线程
}

/* 当前线程将自己阻塞，标志其状态为stat */
void thread_block(task_state_t stat)
{
    ASSERT(stat == TASK_BLOCKED || stat == TASK_WAITING || stat == TASK_HANGING);

    INTR_STATUS_T old_stat = intr_disable();
    struct task_struct *curr_thread = running_thread();

    curr_thread->status = stat; //置其状态为stat
    schedule();                 //将当前线程换下处理器
    /* 待当前线程被解除阻塞后才继续运行下面的代码 */
    intr_set_status(old_stat);
}

/* 将线程pthread解除阻塞 */
void thread_unblock(struct task_struct *pthread)
{
    INTR_STATUS_T old_stat = intr_disable();

    if (pthread->status != TASK_READY) {
        //将被唤醒的线程肯定不在就绪队列中，否则就出错
        if (elem_find(&thread_ready_list, &pthread->general_tag)) {
            PANIC("thread_unblock: blocked thread in ready_list.\n");
        }
        list_push(&thread_ready_list, &pthread->general_tag); //放到就绪队列最前面，使其尽快被调度
        pthread->status = TASK_READY;
    }
    intr_set_status(old_stat);
}

/* 分配pid */
static pid_t allocate_pid(void)
{
    static pid_t next_pid = 0;
    lock_acquire(&pid_lock);
    next_pid++;
    lock_release(&pid_lock);
    return next_pid;
}

/* 主动让出cpu，换其他线程运行 */
void thread_yield(void)
{
    struct task_struct *curr = running_thread();
    INTR_STATUS_T old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &curr->general_tag));
    list_append(&thread_ready_list, &curr->general_tag);
    curr->status = TASK_READY;
    schedule();
    intr_set_status(old_status);
}
