#include "types.h"
#include "global.h"
#include "thread.h"
#include "userprog.h"
#include "mm.h"
#include "debug.h"
#include "tss.h"
#include "kernel.h"
#include "string.h"
#include "interrupt.h"

extern void intr_exit(void);

/*
 * 构建用户进程初始上下文信息
*/
void start_process(void *filename) 
{
    void *function = filename;
    struct task_struct *curr = running_thread();
    curr->self_kstack += sizeof(thread_stack_t);
    intr_stack_t *proc_stack = (intr_stack_t *)curr->self_kstack;

    proc_stack->edi = 0;
    proc_stack->esi = 0;
    proc_stack->ebp = 0;
    proc_stack->esp_dummy = 0;

    proc_stack->ebx = 0;
    proc_stack->edx = 0;
    proc_stack->ecx = 0;
    proc_stack->eax = 0;

    proc_stack->gs = 0;
    proc_stack->fs = SELECTOR_U_DATA;
    proc_stack->es = SELECTOR_U_DATA;
    proc_stack->ds = SELECTOR_U_DATA;

    proc_stack->eip = function;     //待执行的用户程序地址
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
    proc_stack->esp = (void *)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);
    proc_stack->ss = SELECTOR_U_DATA;
    __asm__ volatile("mov %%esp, %0; jmp intr_exit" :: "g"(proc_stack) : "memory"); // intel: mov esp, eax
}

/* 激活页表 */
void page_dir_activate(struct task_struct *pthread)
{
    /*
     * 执行此函数时，当前任务可能是线程。
     * 之所以对线程也要重新安装页表，原因是上一次被调度的可能是进程，
     * 否则不恢复页表的话，线程就会使用进程的页表了
    */

    //若为内核线程，需要重新填充页表为0x100000
    uint32_t pagedir_phy_addr = 0x100000;
    if (pthread->pgdir != NULL) {
        //用户进程有自己的页目录表
        pagedir_phy_addr = addr_v2p((uint32_t)pthread->pgdir);
    }
    //更新页目录寄存器cr3使新页表生效
    __asm__("mov %%cr3, %0" :: "r"(pagedir_phy_addr) : "memory"); // intel: mov cr3, eax
}

/*
 * 激活线程或进程的页表，更新tss中的esp0为进程的特权级0的栈
*/
void process_activate(struct task_struct *pthread)
{
    ASSERT(pthread != NULL);
    page_dir_activate(pthread);

    /*
     * 内核线程特权级本身就是0，处理器进入中断时并不会
     * 从tss中获取0特权级栈地址，故不需要更新esp0
    */
    if (pthread->pgdir != NULL) {
        //更新进程的esp0，用于此进程被中断时保留上下文
        update_tss_esp(pthread);
    }
}

/*
 * 创建页目录表，将当前页表的表示内核空间的pde复制，
 * 成功则返回页目录的虚拟地址，否则返回NULL
*/
uint32_t *create_page_dir(void)
{
    //用户进程的页表不能让用户直接访问到，所以在内核空间来申请
    uint32_t *page_dir_vaddr = get_kernel_pages(1); //页目录表起始地址
    if (page_dir_vaddr == NULL) {
        print_unlock("create_page_dir: get_kernel_page failed!\r\n");
        return NULL;
    }

    //1.复制页表
    memcpy((uint32_t *)((uint32_t)page_dir_vaddr + 0x300*4), (uint32_t *)(0xfffff000 + 0x300*4), 1024);

    //2.更新页目录地址
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);
    //页目录地址是存入在页目录的最后一项，更新页目录地址为新页目录的物理地址
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;
    page_dir_vaddr[0] = page_dir_vaddr[768];    //必须要有这一行代码，否则切换页表时(给cr3赋值)如果page_dir_vaddr[0]=0则会引起page fault导致系统重启

    return page_dir_vaddr;
}

/*
 * 创建用户进程虚拟地址位图
*/
void create_user_vaddr_bitmap(struct task_struct *user_prog)
{
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

/*
 * 创建用户进程
*/
void process_execute(void *filename, char *name, uint8_t prio)
{
    //pcb内核的数据结构，由内核来维护进程信息，因此要在内核内存池中申请
    struct task_struct *thread = get_kernel_pages(1);

    init_thread(thread, name, prio);
    create_user_vaddr_bitmap(thread);
    thread_create(thread, start_process, filename);
    thread->pgdir = create_page_dir();

    INTR_STATUS_T old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);

    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);
    intr_set_status(old_status);
}
