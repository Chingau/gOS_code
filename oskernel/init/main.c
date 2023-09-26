#include "kernel.h"
#include "traps.h"
#include "string.h"
#include "tty.h"
#include "mm.h"
#include "system.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"
#include "timer.h"
#include "keyboard.h"
#include "tss.h"
#include "userprog.h"
#include "syscall_int.h"
#include "syscall.h"
#include "stdio.h"
#include "ide.h"
#include "fs.h"
#include "dir.h"
#include "shell.h"
#include "global.h"

/* init 进程 */
void init(void)
{
    uint32_t ret_pid = fork();
    if (ret_pid) {
        while (1);
    } else {
        //子进程
        my_shell();
    }
    PANIC("init:should not be here.");
}

void kernel_main(void)
{
    console_init();
    tss_init();
    idt_init();
    check_memory();
    mem_init();
    timer_init();
    keyboard_init();
    thread_init();
    console_lock_init();
    syscall_init();
    ide_init();
    filesys_init();
    print_unlock("hello gos, init done.\n");
    console_clear();
    printk("[gaoxu@localhost:/]$ ");
    intr_enable();

    //创建两个内核线程
    //thread_start("consumer_a", 10, k_thread_a, "argA");
    //thread_start("consumer_b", 10, k_thread_b, "argB");

    //process_execute(u_prog_a, "u_prog_a", 20);
    //process_execute(u_prog_b, "u_prog_b", 20);

    //写入应用程序
    // uint32_t file_size = 14900;
    // uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);
    // struct disk *sda = &channels[0].devices[0]; //有操作系统的裸盘
    // void *prog_buf = sys_malloc(file_size);
    // if (prog_buf == NULL) {
    //     printk("malloc error.\n");
    //     while(1);
    // }
    // ide_read(sda, 300, prog_buf, sec_cnt);
    // int32_t fd = sys_open("/prog_arg", O_RDWR|O_CREAT);
    // if (fd != -1) {
    //     if (sys_write(fd, prog_buf, file_size) == -1) {
    //         printk("file write error.\n");
    //         while (1);
    //     }
    // }
    // sys_close(fd);
    // sys_free(prog_buf);

    //写入应用程序结束
    console_clear();
    printk("[gaoxu@localhost:/]$ ");
    while (1);
    //BOCHS_DEBUG_MAGIC
}
