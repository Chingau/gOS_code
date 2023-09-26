#include "thread.h"
#include "mm.h"
#include "string.h"
#include "kernel.h"
#include "interrupt.h"
#include "debug.h"
#include "global.h"
#include "userprog.h"
#include "sync.h"
#include "file.h"
#include "fs.h"
#include "stdio.h"

extern void switch_to(struct task_struct *curr, struct task_struct *next);
extern void init(void);

struct task_struct *main_thread;        //主线程PCB
struct task_struct *idle_thread;        //idle线程
struct list thread_ready_list;          //就绪队列
struct list thread_all_list;            //所有任务队列
static struct list_elem *thread_tag;

/* pid的位图，最大支持1024个pid */
uint8_t pid_bitmap_bits[128] = {0};
/* pid 池 */
struct pid_pool {
    bitmap_t pid_bitmap;    //pid位图
    uint32_t pid_start;     //起始pid
    lock_t pid_lock;        //分配pid锁
} pid_pool;

//函数声明
static void pid_pool_init(void);
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
    //预留标准输入输出
    pthread->fd_table[0] = 0;
    pthread->fd_table[1] = 1;
    pthread->fd_table[2] = 2;
    uint8_t fd_idx = 3;
    while (fd_idx < MAX_FILES_OPEN_PRE_PROC) {
        pthread->fd_table[fd_idx] = -1;
        fd_idx++;
    }
    pthread->cwd_inode_nr = 0;  //以根目录作为默认工作路径
    pthread->parent_pid = -1;   //父进程pid默认为-1
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
    //print_unlock("%s thread vaddr:0x%08x\r\n", name, thread);

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
    pid_pool_init();

    //先创建第一个用户进程:init,使其pid=1
    process_execute(init, "init", 10);
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

pid_t fork_pid(void)
{
    return allocate_pid();
}

/* 以填空格的方式输出 buf */
static void pad_print(char *buf, int32_t buf_len, void *ptr, char format)
{
    memset(buf, 0, buf_len);
    uint8_t out_pad_0idx = 0;

    switch (format) {
        case 's':
            out_pad_0idx = sprintf(buf, "%s", ptr);
        break;
        case 'd':
            out_pad_0idx = sprintf(buf, "%d", *((uint16_t *)ptr));
        break;
        case 'x':
            out_pad_0idx = sprintf(buf, "%x", *((uint32_t *)ptr));
    }
    while (out_pad_0idx < buf_len) {    //以空格填充
        buf[out_pad_0idx] = ' ';
        out_pad_0idx++;
    }
    sys_write(stdout_no, buf, buf_len - 1);
}

/* 用于在list_traversal函数中的回调函数，用于针对线程队列的处理 */
static bool elem2thread_info(struct list_elem *pelem, int arg UNUSED)
{
    struct task_struct *pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    char out_pad[16] = {0};

    pad_print(out_pad, 16, &pthread->pid, 'd');
    if (pthread->parent_pid == -1) {
        pad_print(out_pad, 16, "NULL", 's');
    } else {
        pad_print(out_pad, 16, &pthread->parent_pid, 'd');
    }

    switch(pthread->status) {
        case TASK_RUNNING:
            pad_print(out_pad, 16, "RUNNING", 's');
        break;
        case TASK_READY:
            pad_print(out_pad, 16, "READY", 's');
        break;
        case TASK_BLOCKED:
            pad_print(out_pad, 16, "BLOCKED", 's');
        break;
        case TASK_WAITING:
            pad_print(out_pad, 16, "WAITING", 's');
        break;
        case TASK_HANGING:
            pad_print(out_pad, 16, "HANGING", 's');
        break;
        case 5:
            pad_print(out_pad, 16, "DIED", 's');
        break;
    }
    pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

    memset(out_pad, 0, 16);
    ASSERT(strlen(pthread->name) < 17);
    memcpy(out_pad, pthread->name, strlen(pthread->name));
    strcat(out_pad, "\n");
    sys_write(stdout_no, out_pad, strlen(out_pad));
    return false;
}

void sys_ps(void)
{
    char *ps_tittle = "PID             PPID            STAT            TICKS           COMMAND\n";
    sys_write(stdout_no, ps_tittle, strlen(ps_tittle));
    list_traversal(&thread_all_list, elem2thread_info, 0);
}

/* 初始化 pid 池 */
static void pid_pool_init(void)
{
    pid_pool.pid_start = 1;
    pid_pool.pid_bitmap.bits = pid_bitmap_bits;
    pid_pool.pid_bitmap.btmp_bytes_len = sizeof(pid_bitmap_bits);
    bitmap_init(&pid_pool.pid_bitmap);
    lock_init(&pid_pool.pid_lock);
}

/* 分配pid */
static pid_t allocate_pid(void)
{
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
    lock_release(&pid_pool.pid_lock);
    return (bit_idx + pid_pool.pid_start);
}

/* 释放pid */
void release_pid(pid_t pid)
{
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = pid - pid_pool.pid_start;
    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
    lock_release(&pid_pool.pid_lock);
}

/* 回收thread_over的pcb和页表，并将其从调度队列中去除 */
void thread_exit(struct task_struct *thread_over, bool need_schedule)
{
    intr_disable();
    thread_over->status = TASK_DIED;

    //如果thread_over不是当前线程，就有可能还在就绪队列中，将其从中删除
    if (elem_find(&thread_ready_list, &thread_over->general_tag)) {
        list_remove(&thread_over->general_tag);
    }

    if (thread_over->pgdir) {
        mfree_page(PF_KERNEL, thread_over->pgdir, 1);   //如果是进程，回收进程的页表
    }

    list_remove(&thread_over->all_list_tag);

    //回收pcb所在的页，主线程的pcb不在堆中，跳过
    if (thread_over != main_thread) {
        mfree_page(PF_KERNEL, thread_over, 1);
    }

    release_pid(thread_over->pid);

    //如果需要下一轮调度则主动调用schedule
    if (need_schedule) {
        schedule();
        PANIC("thread_exit: should not be here.\n");
    }
}

/* 比对任务的pid */
static bool pid_check(struct list_elem *pelem, int32_t pid)
{
    struct task_struct *pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    if (pthread->pid == pid) {
        return true;
    }
    return false;
}

/* 根据pid找pcb，若找到则返回该pcb，否则返回NULL */
struct task_struct *pid2thread(int32_t pid)
{
    struct list_elem *pelem = list_traversal(&thread_all_list, pid_check, pid);
    if (pelem == NULL)
        return NULL;

    struct task_struct *thread = elem2entry(struct task_struct, all_list_tag, pelem);
    return thread;
}
