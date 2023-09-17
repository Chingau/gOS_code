[bits 32]
[SECTION .text]

TI_GDT equ 000b
TI_LDT equ 100b
RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
PRL3 equ 11b
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

global cls_screen
cls_screen:
    pushad
    ;由于用户程序cpl为3，显存段的dpl为0
    ;故用于显存的选择子gs在低于自己特权的环境中为0
    ;导致用户程序再次进入中断后，gs为0
    ;故直接在put_str中每次都为gs赋值
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    mov ebx, 0
    mov ecx, 80*25
.cls:
    mov word [gs:ebx], 0x720    ;0x720是黑底白字的空格键
    add ebx, 2
    loop .cls
    mov ebx, 0

.set_cursor:
    ;1.先设置高8位
    mov dx, 0x03d4  ;索引寄存器
    mov al, 0x0e    ;用于提供光标位置的高8位
    out dx, al
    mov dx, 0x03d5  ;通过读写数据端口0x03d5来获得或设置光标位置
    mov al, bh
    out dx, al

    ;2.再设置低8位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al
    popad
    ret