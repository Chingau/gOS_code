/*
 * Create by gaoxu on 2023.07.05
 * */

#ifndef __GOS_OSKERNEL_THREAD_H__
#define __GOS_OSKERNEL_THREAD_H__
#include "types.h"

typedef void thread_func(void *);

/* 进程或线程的状态 */
typedef enum {
    TASK_RUNNING,
    TASL_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING
} task_state_t;

/* 中断栈 intr_stack_t */
/*
 此结构用于中断发生时保护程序(线程或进程)的上下文环境：
 进程或线程被外部中断或软中断打断时，会按照此结构压入上下文
 寄存器，intr_exit 中的出栈操作是此结构的逆操作
 此栈在线程自己的内核中位置固定，所在页的最顶端
*/
typedef struct {
    uint32_t vec_no;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    //虽然 pushad 把 esp 也压入，但 esp 是不断变化的，所以会被 popad 忽略
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    /* 以下参数是CPU由低特权级进入高特权级时压入 */
    uint32_t err_code;      //err_code会被压入到eip之后
    void (*eip)(void);
    uint32_t cs;
    uint32_t eflags;
    void *esp;
    uint32_t ss;
} intr_stack_t;


/* 线程栈 thread_stack_t */
/*
 线程自己的栈，用于存储线程中待执行的函数
 此结构在线程自己的内核栈中位置不固定
 仅用在 switch_to 时保存线程环境
 实际位置取决于实际运行情况
*/
typedef struct {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    /*
     线程第一次执行时，eip指向待调用的函数 kernel_thread
     其它时候，eip是指向 switch_to 的返回地址
    */
    void (*eip)(thread_func *func, void *func_arg);

    /* 以下参数仅供线程第一次被调试上CPU运行时使用 */
    void *unused_retaddr;       //此参数只为占位置充当函数返回地址
    thread_func *func;
    void *func_arg;
} thread_stack_t;


/* 进程或线程的 PCB */
struct task_struct {
    uint32_t *self_kstack;      //各内核线程都用自己的内核栈  
    task_state_t status;        //线程状态
    uint8_t priority;           //线程优先级
    char name[16];              //线程名
    uint32_t stack_magic;       //栈的边界标记，用于检测栈的溢出
};

struct task_struct* thread_start(char *name, uint8_t prio, thread_func *func, void *func_arg);
#endif
