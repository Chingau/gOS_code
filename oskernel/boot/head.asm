[SECTION .text]
[BITS 32]
extern kernel_main
global _start

_start:
;配置8259a芯片，响应中断用
.config_8259a:
    ;向主片发送ICW1
    mov al, 11h
    out 20h, al
    ;向从片发送ICW1
    out 0a0h, al

    ;向主片发送ICW2
    mov al, 20h
    out 21h, al
    ;向从片发送ICW2
    mov al, 28h
    out 0a1h, al

    ;向主片发送ICW3
    mov al, 04h
    out 21h, al
    ;向从片发送ICW3
    mov al, 02h
    out 0a1h, al

    ;向主片发送ICW4
    mov al, 03h
    out 21h, al
    ;向从片发送ICW4
    out 0a1h, al

    ;向主片发送OCW1，接收键盘和时钟中断
.enable_8259a_master:
    mov al, 1111_1100b
    out 21h, al

    ;向从片发送OCW1，屏蔽从片所有中断响应
.disable_8259a_slave:
    mov al, 1111_1111b
    out 0a1h, al

.enter_c_world:
    xchg bx, bx
    call kernel_main
    jmp $