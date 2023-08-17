[bits 32]
extern intr_table

%define ERROR_CODE nop;不可屏蔽中断发生时如果CPU压入了错误码，则这里什么也不做
%define ZERO push 0;中断发生时若CPU未压入错误码，则这里我们手动压入一个假的错误码来占位

[SECTION .text]
global interrupt_handler_table

;压入栈的值 从高地址 到 低地址 依次为：
; eflags
; cs
; eip
; error code
; -----以上四个值是进入中断时CPU自动压入-----
; ds
; es
; fs
; gs
; -----  -----
; eax
; ecx
; edx
; ebx
; esp
; ebp
; esi
; edi
; -----以上8个值是执行pushad压入的-----
; vec_nr
%macro INTERRUPT_HANDLER 2
interrupt_handler_%1:
    %2
    ;以下是保存上下文环境
    push ds
    push es
    push fs
    push gs
    pushad

    ;如果是从片上进入的中断，除了往从片上发送EOI外，主片上也要发送EOI
    mov al, 0x20 ;中断结束命令EOI
    out 0xa0, al ;向从片发送
    out 0x20, al ;向主片发送

    push %1
    call [intr_table + %1*4]
    jmp intr_exit
%endmacro

global intr_exit
intr_exit:
    ;以下是恢复上下文环境
    add esp, 4 ;这里平4个字节的栈是为了让pushad和popad能成对使用，因为在pushad和popad中间多push %1（多压4字节）
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4 ;手动跳过错误码
    iretd

INTERRUPT_HANDLER 0x00, ZERO; divide by zero
INTERRUPT_HANDLER 0x01, ZERO; debug
INTERRUPT_HANDLER 0x02, ZERO; non maskable interrupt
INTERRUPT_HANDLER 0x03, ZERO; breakpoint

INTERRUPT_HANDLER 0x04, ZERO; overflow
INTERRUPT_HANDLER 0x05, ZERO; bound range exceeded
INTERRUPT_HANDLER 0x06, ZERO; invalid opcode
INTERRUPT_HANDLER 0x07, ZERO; device not avilable

INTERRUPT_HANDLER 0x08, ERROR_CODE; double fault
INTERRUPT_HANDLER 0x09, ZERO; coprocessor segment overrun
INTERRUPT_HANDLER 0x0a, ERROR_CODE; invalid TSS
INTERRUPT_HANDLER 0x0b, ERROR_CODE; segment not present

INTERRUPT_HANDLER 0x0c, ERROR_CODE; stack segment fault
INTERRUPT_HANDLER 0x0d, ERROR_CODE; general protection fault
INTERRUPT_HANDLER 0x0e, ERROR_CODE; page fault
INTERRUPT_HANDLER 0x0f, ZERO; reserved

INTERRUPT_HANDLER 0x10, ZERO; x87 floating point exception
INTERRUPT_HANDLER 0x11, ERROR_CODE; alignment check
INTERRUPT_HANDLER 0x12, ZERO; machine check
INTERRUPT_HANDLER 0x13, ZERO; SIMD Floating - Point Exception

INTERRUPT_HANDLER 0x14, ZERO; Virtualization Exception
INTERRUPT_HANDLER 0x15, ZERO; Control Protection Exception

INTERRUPT_HANDLER 0x16, ZERO; reserved
INTERRUPT_HANDLER 0x17, ZERO; reserved
INTERRUPT_HANDLER 0x18, ZERO; reserved
INTERRUPT_HANDLER 0x19, ZERO; reserved
INTERRUPT_HANDLER 0x1a, ZERO; reserved
INTERRUPT_HANDLER 0x1b, ZERO; reserved
INTERRUPT_HANDLER 0x1c, ZERO; reserved
INTERRUPT_HANDLER 0x1d, ZERO; reserved
INTERRUPT_HANDLER 0x1e, ZERO; reserved
INTERRUPT_HANDLER 0x1f, ZERO; reserved

INTERRUPT_HANDLER 0x20, ZERO; clock 时钟中断
INTERRUPT_HANDLER 0x21, ZERO; 键盘中断
INTERRUPT_HANDLER 0x22, ZERO
INTERRUPT_HANDLER 0x23, ZERO
INTERRUPT_HANDLER 0x24, ZERO
INTERRUPT_HANDLER 0x25, ZERO
INTERRUPT_HANDLER 0x26, ZERO
INTERRUPT_HANDLER 0x27, ZERO
INTERRUPT_HANDLER 0x28, ZERO; rtc 实时时钟
INTERRUPT_HANDLER 0x29, ZERO
INTERRUPT_HANDLER 0x2a, ZERO
INTERRUPT_HANDLER 0x2b, ZERO
INTERRUPT_HANDLER 0x2c, ZERO
INTERRUPT_HANDLER 0x2d, ZERO
INTERRUPT_HANDLER 0x2e, ZERO
INTERRUPT_HANDLER 0x2f, ZERO

[SECTION .data]
interrupt_handler_table:
    dd interrupt_handler_0x00
    dd interrupt_handler_0x01
    dd interrupt_handler_0x02
    dd interrupt_handler_0x03
    dd interrupt_handler_0x04
    dd interrupt_handler_0x05
    dd interrupt_handler_0x06
    dd interrupt_handler_0x07
    dd interrupt_handler_0x08
    dd interrupt_handler_0x09
    dd interrupt_handler_0x0a
    dd interrupt_handler_0x0b
    dd interrupt_handler_0x0c
    dd interrupt_handler_0x0d
    dd interrupt_handler_0x0e
    dd interrupt_handler_0x0f
    dd interrupt_handler_0x10
    dd interrupt_handler_0x11
    dd interrupt_handler_0x12
    dd interrupt_handler_0x13
    dd interrupt_handler_0x14
    dd interrupt_handler_0x15
    dd interrupt_handler_0x16
    dd interrupt_handler_0x17
    dd interrupt_handler_0x18
    dd interrupt_handler_0x19
    dd interrupt_handler_0x1a
    dd interrupt_handler_0x1b
    dd interrupt_handler_0x1c
    dd interrupt_handler_0x1d
    dd interrupt_handler_0x1e
    dd interrupt_handler_0x1f
    dd interrupt_handler_0x20
    dd interrupt_handler_0x21
    dd interrupt_handler_0x22
    dd interrupt_handler_0x23
    dd interrupt_handler_0x24
    dd interrupt_handler_0x25
    dd interrupt_handler_0x26
    dd interrupt_handler_0x27
    dd interrupt_handler_0x28
    dd interrupt_handler_0x29
    dd interrupt_handler_0x2a
    dd interrupt_handler_0x2b
    dd interrupt_handler_0x2c
    dd interrupt_handler_0x2d
    dd interrupt_handler_0x2e
    dd interrupt_handler_0x2f
