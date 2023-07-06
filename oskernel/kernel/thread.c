#include "thread.h"
#include "mm.h"
#include "string.h"
#include "kernel.h"

#define PG_SIZE 4096

static void kernel_thread(thread_func *function, void *arg)
{
    function(arg);
}

static void thread_init(struct task_struct* pthread, char *name, uint8_t prio)
{
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);
    pthread->status = TASK_RUNNING;
    pthread->priority = prio;
    // self_kstack是线程自己在内核态下使用的栈顶地址
    pthread->self_kstack = (uint32_t *)((uint32_t)pthread + PG_SIZE);
    pthread->stack_magic = 0x20000324;
}

static void thread_create(struct task_struct* pthread, thread_func *function, void *arg)
{
    pthread->self_kstack -= sizeof(intr_stack_t);   //预留出中断栈的空间
    pthread->self_kstack -= sizeof(thread_stack_t); //预留出线程栈的空间

    thread_stack_t *kthread_stack = (thread_stack_t *)pthread->self_kstack;
    
    /* 
     注意，下面初始化的排列顺序就是在栈中的样子，从高地址到低地址，ebp那个位置是栈顶，
     配合 thread_start 函数中的 __asm__ 那串汇编代码，可以发现其设计思想
    */
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
    printk("%s thread vaddr:0x%08x\r\n", name, thread);

    thread_init(thread, name, prio);
    thread_create(thread, func, func_arg);
    //我艹，这个位置坑我很久，第一条汇编最开始我写的是 mov %0, %%esp，这不是ATT的风格吗？
    //为什么转换成intel风格后发现变成了 mov eax, esp，导致page fault
    //最终改成 mov %%esp, %0 才成功。
    __asm__ volatile("mov %%esp, %0;"
            "pop %%ebp;"
            "pop %%ebx;"
            "pop %%edi;"
            "pop %%esi;"
            "ret;"
            ::"g"(thread->self_kstack):"memory");
    return thread;
}
