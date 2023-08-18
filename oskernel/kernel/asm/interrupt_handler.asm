[bits 32]
extern intr_table
extern syscall_table

%define ERROR_CODE nop;不可屏蔽中断发生时如果CPU压入了错误码，则这里什么也不做
%define ZERO push 0;中断发生时若CPU未压入错误码，则这里我们手动压入一个假的错误码来占位

[SECTION .text]
global interrupt_handler_table
global intr_exit
global syscall_handler

syscall_handler:
    ; 保存上下文环境
    push 0  ;压入错误码，保存格式统一
    push ds
    push es
    push fs
    push gs
    pushad

    push 0x80   ;保持统一的栈格式，中断向量号

    push edx    ;系统调用中第3个参数
    push ecx    ;系统调用中第2个参数
    push ebx    ;系统调用中第1个参数

    ;调用子功能处理函数
    call [syscall_table + eax*4]
    add esp, 12     ;跨过上面的三个参数

    ;将call调用后的返回值存入当前内核栈中的eax的位置
    mov [esp + 8*4], eax
    jmp intr_exit       ;恢复上下文环境


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

INTERRUPT_HANDLER 0x30, ZERO
INTERRUPT_HANDLER 0x31, ZERO
INTERRUPT_HANDLER 0x32, ZERO
INTERRUPT_HANDLER 0x33, ZERO
INTERRUPT_HANDLER 0x34, ZERO
INTERRUPT_HANDLER 0x35, ZERO
INTERRUPT_HANDLER 0x36, ZERO
INTERRUPT_HANDLER 0x37, ZERO
INTERRUPT_HANDLER 0x38, ZERO
INTERRUPT_HANDLER 0x39, ZERO
INTERRUPT_HANDLER 0x3a, ZERO
INTERRUPT_HANDLER 0x3b, ZERO
INTERRUPT_HANDLER 0x3c, ZERO
INTERRUPT_HANDLER 0x3d, ZERO
INTERRUPT_HANDLER 0x3e, ZERO
INTERRUPT_HANDLER 0x3f, ZERO

INTERRUPT_HANDLER 0x40, ZERO
INTERRUPT_HANDLER 0x41, ZERO
INTERRUPT_HANDLER 0x42, ZERO
INTERRUPT_HANDLER 0x43, ZERO
INTERRUPT_HANDLER 0x44, ZERO
INTERRUPT_HANDLER 0x45, ZERO
INTERRUPT_HANDLER 0x46, ZERO
INTERRUPT_HANDLER 0x47, ZERO
INTERRUPT_HANDLER 0x48, ZERO
INTERRUPT_HANDLER 0x49, ZERO
INTERRUPT_HANDLER 0x4a, ZERO
INTERRUPT_HANDLER 0x4b, ZERO
INTERRUPT_HANDLER 0x4c, ZERO
INTERRUPT_HANDLER 0x4d, ZERO
INTERRUPT_HANDLER 0x4e, ZERO
INTERRUPT_HANDLER 0x4f, ZERO

INTERRUPT_HANDLER 0x50, ZERO
INTERRUPT_HANDLER 0x51, ZERO
INTERRUPT_HANDLER 0x52, ZERO
INTERRUPT_HANDLER 0x53, ZERO
INTERRUPT_HANDLER 0x54, ZERO
INTERRUPT_HANDLER 0x55, ZERO
INTERRUPT_HANDLER 0x56, ZERO
INTERRUPT_HANDLER 0x57, ZERO
INTERRUPT_HANDLER 0x58, ZERO
INTERRUPT_HANDLER 0x59, ZERO
INTERRUPT_HANDLER 0x5a, ZERO
INTERRUPT_HANDLER 0x5b, ZERO
INTERRUPT_HANDLER 0x5c, ZERO
INTERRUPT_HANDLER 0x5d, ZERO
INTERRUPT_HANDLER 0x5e, ZERO
INTERRUPT_HANDLER 0x5f, ZERO

INTERRUPT_HANDLER 0x60, ZERO
INTERRUPT_HANDLER 0x61, ZERO
INTERRUPT_HANDLER 0x62, ZERO
INTERRUPT_HANDLER 0x63, ZERO
INTERRUPT_HANDLER 0x64, ZERO
INTERRUPT_HANDLER 0x65, ZERO
INTERRUPT_HANDLER 0x66, ZERO
INTERRUPT_HANDLER 0x67, ZERO
INTERRUPT_HANDLER 0x68, ZERO
INTERRUPT_HANDLER 0x69, ZERO
INTERRUPT_HANDLER 0x6a, ZERO
INTERRUPT_HANDLER 0x6b, ZERO
INTERRUPT_HANDLER 0x6c, ZERO
INTERRUPT_HANDLER 0x6d, ZERO
INTERRUPT_HANDLER 0x6e, ZERO
INTERRUPT_HANDLER 0x6f, ZERO

INTERRUPT_HANDLER 0x70, ZERO
INTERRUPT_HANDLER 0x71, ZERO
INTERRUPT_HANDLER 0x72, ZERO
INTERRUPT_HANDLER 0x73, ZERO
INTERRUPT_HANDLER 0x74, ZERO
INTERRUPT_HANDLER 0x75, ZERO
INTERRUPT_HANDLER 0x76, ZERO
INTERRUPT_HANDLER 0x77, ZERO
INTERRUPT_HANDLER 0x78, ZERO
INTERRUPT_HANDLER 0x79, ZERO
INTERRUPT_HANDLER 0x7a, ZERO
INTERRUPT_HANDLER 0x7b, ZERO
INTERRUPT_HANDLER 0x7c, ZERO
INTERRUPT_HANDLER 0x7d, ZERO
INTERRUPT_HANDLER 0x7e, ZERO
INTERRUPT_HANDLER 0x7f, ZERO

INTERRUPT_HANDLER 0x80, ZERO

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
    dd interrupt_handler_0x30
    dd interrupt_handler_0x31
    dd interrupt_handler_0x32
    dd interrupt_handler_0x33
    dd interrupt_handler_0x34
    dd interrupt_handler_0x35
    dd interrupt_handler_0x36
    dd interrupt_handler_0x37
    dd interrupt_handler_0x38
    dd interrupt_handler_0x39
    dd interrupt_handler_0x3a
    dd interrupt_handler_0x3b
    dd interrupt_handler_0x3c
    dd interrupt_handler_0x3d
    dd interrupt_handler_0x3e
    dd interrupt_handler_0x3f
    dd interrupt_handler_0x40
    dd interrupt_handler_0x41
    dd interrupt_handler_0x42
    dd interrupt_handler_0x43
    dd interrupt_handler_0x44
    dd interrupt_handler_0x45
    dd interrupt_handler_0x46
    dd interrupt_handler_0x47
    dd interrupt_handler_0x48
    dd interrupt_handler_0x49
    dd interrupt_handler_0x4a
    dd interrupt_handler_0x4b
    dd interrupt_handler_0x4c
    dd interrupt_handler_0x4d
    dd interrupt_handler_0x4e
    dd interrupt_handler_0x4f
    dd interrupt_handler_0x50
    dd interrupt_handler_0x51
    dd interrupt_handler_0x52
    dd interrupt_handler_0x53
    dd interrupt_handler_0x54
    dd interrupt_handler_0x55
    dd interrupt_handler_0x56
    dd interrupt_handler_0x57
    dd interrupt_handler_0x58
    dd interrupt_handler_0x59
    dd interrupt_handler_0x5a
    dd interrupt_handler_0x5b
    dd interrupt_handler_0x5c
    dd interrupt_handler_0x5d
    dd interrupt_handler_0x5e
    dd interrupt_handler_0x5f
    dd interrupt_handler_0x60
    dd interrupt_handler_0x61
    dd interrupt_handler_0x62
    dd interrupt_handler_0x63
    dd interrupt_handler_0x64
    dd interrupt_handler_0x65
    dd interrupt_handler_0x66
    dd interrupt_handler_0x67
    dd interrupt_handler_0x68
    dd interrupt_handler_0x69
    dd interrupt_handler_0x6a
    dd interrupt_handler_0x6b
    dd interrupt_handler_0x6c
    dd interrupt_handler_0x6d
    dd interrupt_handler_0x6e
    dd interrupt_handler_0x6f
    dd interrupt_handler_0x70
    dd interrupt_handler_0x71
    dd interrupt_handler_0x72
    dd interrupt_handler_0x73
    dd interrupt_handler_0x74
    dd interrupt_handler_0x75
    dd interrupt_handler_0x76
    dd interrupt_handler_0x77
    dd interrupt_handler_0x78
    dd interrupt_handler_0x79
    dd interrupt_handler_0x7a
    dd interrupt_handler_0x7b
    dd interrupt_handler_0x7c
    dd interrupt_handler_0x7d
    dd interrupt_handler_0x7e
    dd interrupt_handler_0x7f
    dd interrupt_handler_0x80
