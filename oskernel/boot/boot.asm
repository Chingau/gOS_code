;0柱面0磁道1扇区
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

    mov dx, 0x1f2       ;指定读取或定入的扇区数
    mov al, 1           ;读取1个扇区, 根据setup.asm文件的大小实际设置
    out dx, al          ;操作硬盘寄存器

    mov ecx, 2          ;从硬盘的第2扇区开始读
    ;设置LBA的地址
    mov dx, 0x1f3
    mov al, cl
    out dx, al          ;LBA地址的低8位

    mov dx, 0x1f4
    mov al, ch
    out dx, al          ;LBA地址的中8位

    mov dx, 0x1f5
    shr ecx, 16
    mov al, cl
    out dx, al          ;LBA地址的高8位

    ;bit0~3: LBA地址的24~27位
    ;bit4: 0-主盘 1-从盘
    ;bit5, bit7: 固定为1
    ;bit6: 1-LBA模式 0-CHS模式
    xchg bx, bx
    mov dx, 0x1f6
    mov al, ch
    ;and al, 0b00001111
    ;or al, 0b11100000       ;主盘，LBA模式
    and al, 0b1110_1111
    out dx, al

    ;命令或状态端口
    mov dx, 0x1f7
    mov al, 0x20        ;读命令，即以读的方式操作硬盘
    out dx, al          ;断点

    ;验证状态
    ;bit3: 0-表示硬盘未准备好 1-准备好了
    ;bit7: 0-表示硬盘不忙 1-表示硬盘忙
    ;bit0: 0-表示前一条指令正常执行 1-表示执行出错，出错信息通过0x1f1端口获得
.read_check:
    mov dx, 0x1f7
    in al, dx
    and al, 0b10001000      ;取硬盘状态的bit3,bit7
    cmp al, 0b00001000      ;硬盘数据已准备好且不忙
    jnz .read_check
    ;读数据
    mov dx, 0x1f0
    mov cx, 256
    mov edi, SETUP_MAIN_ADDR
.read_data:
    in ax, dx
    mov [edi], ax
    add edi, 2
    loop .read_data

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
    db "read disk error!", 10, 13, 0      ;read disk error!\r\n0

jmp_to_setup:
    db "jump to setup...", 10, 13, 0        ;jump to setup...\r\n0

;clear zero
times 510 - ($ - $$) db 0
dw 0xaa55