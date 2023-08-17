[bits 32]
[SECTION .text]
global switch_to

switch_to:
    ;程序运行到此处时，当前线程栈从高到低地址依次保存的是：
    ; ... schedule函数返回值, next_pcb, curr_pcb, switch_to返回值
    push esi
    push edi
    push ebx
    push ebp
    mov eax, [esp + 20]     ;curr_pcb = esp+20
    mov [eax], esp          ;[eax]就是取出curr_pcb结构体的第一个成员，即self_kstack，即保存当前esp到curr.self_ksta

    ;上面代码是保存curr线程环境，下面代码就是恢复next线程环境
    mov eax, [esp + 24]     ;next_pcb = esp+24
    mov esp, [eax]          ;把next.self_kstack中保存的栈地址恢复到当前esp中
    pop ebp                 ;注意，这4个寄存器的恢复并不是上面push的那4个寄存器
    pop ebx
    pop edi
    pop esi
    ret