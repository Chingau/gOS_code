[ORG  0x7c00]

[SECTION .data]
SETUP_MAIN_ADDR equ 0x500                   ;内核加载器在内存中的位置

[SECTION .text]
[BITS 16]
global _start
_start:
    ; 设置屏幕模式为文本模式，清除屏幕
    mov ax, 3
    int 0x10

    ;读软盘
    mov ch, 0                   ;0柱面
    mov dh, 0                   ;0磁头
    mov cl, 2                   ;第2扇区
    mov bx, SETUP_MAIN_ADDR     ;数据往哪读
    mov ah, 0x02                ;读操作
    mov al, 1                   ;连续读几个扇区
    mov dl, 0                   ;驱动器编号
    int 0x13

    mov si, jmp_to_setup
    call print
    xchg bx, bx

    jmp SETUP_MAIN_ADDR         ;移交控制权

    ;正常情况下不会执行到下面3行代码
    mov si, read_floppy_error
    call print
    jmp $

; 如何调用
; mov     si, msg   ; 1 传入字符串
; call    print     ; 2 调用
; 函数原型：print(register si)
print:
    mov ah, 0x0e
    mov bh, 0
    mov bl, 0x01
.loop:
    mov al, [si]
    cmp al, 0
    jz .done
    int 0x10
    inc si
    jmp .loop
.done:
    ret

read_floppy_error:
    db "read floppy error!", 10, 13, 0      ;read floppy error!\r\n0

jmp_to_setup:
    db "jump to setup...", 10, 13, 0        ;jump to setup...\r\n0

;clear zero
times 510 - ($ - $$) db 0
dw 0xaa55