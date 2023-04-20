;0柱面0磁道1扇区
[ORG  0x7c00]

[SECTION .data]
SETUP_MAIN_ADDR equ 0x500                   ;内核加载器在内存中的位置

[SECTION .text]
[BITS 16]
global boot_start
boot_start:
    ; 设置屏幕模式为文本模式，清除屏幕
    mov ax, 3
    int 0x10

    mov ecx, 1          ;从硬盘的哪个扇区开始读，注意，如果是LBA模式，那该值为1(逻辑扇区从0开始计)；如果是CHS模式，那该值为2
    mov bl, 2           ;读取的扇区数量
    call read_hd

    mov si, jmp_to_setup
    call print
    xchg bx, bx

    jmp SETUP_MAIN_ADDR         ;移交控制权

;------------------------------------------------;
;读硬盘函数
;入参：
;   ecx:从硬盘的第几个扇区开始读
;   bl:读取的扇区数量
;------------------------------------------------;
read_hd:
    mov dx, 0x1f2       ;指定读取或写入的扇区数
    mov al, bl          ;读取1个扇区, 根据setup.asm文件的大小实际设置
    out dx, al          ;操作硬盘寄存器

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
    mov dx, 0x1f6
    shr ecx, 8
    and cl, 0b0000_1111
    mov al, 0b1110_0000
    or al, cl
    out dx, al

    ;命令或状态端口
    mov dx, 0x1f7
    mov al, 0x20        ;读命令，即以读的方式操作硬盘
    out dx, al

    xor ecx, ecx        ;对ecx清0
    mov cl, bl          ;loop的循环次数，读多少扇区就循环多少次
    mov edi, SETUP_MAIN_ADDR    ;从硬盘读取到的数据存放在内在的起始地址处
.start_read:
    push cx
    call .wait_hd_prepare
    call read_hd_data

    pop cx
    loop .start_read
    ret

;验证状态
;bit3: 0-表示硬盘未准备好 1-准备好了
;bit7: 0-表示硬盘不忙 1-表示硬盘忙
;bit0: 0-表示前一条指令正常执行 1-表示执行出错，出错信息通过0x1f1端口获得
.wait_hd_prepare:
    mov dx, 0x1f7
.check:
    in al, dx
    and al, 0b1000_1000      ;取硬盘状态的bit3,bit7
    cmp al, 0b0000_1000      ;硬盘数据已准备好且不忙
    jnz .check
    ret

;------------------------------------------------;
;读硬盘，一次读取两个字节，读256次，即一个扇区
;入参：
;   edi:从硬盘读取的数据保存在内存的起始地址
;------------------------------------------------;
read_hd_data:
    mov dx, 0x1f0
    mov cx, 256
.read_word:
    in ax, dx
    mov [edi], ax
    add edi, 2
    loop .read_word
    ret

;------------------------------------------------;
;打印字符
;入参：
;   si:传入字符串的首地址，字符串要以0结尾
;------------------------------------------------;
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

jmp_to_setup:
    db "jump to setup...", 10, 13, 0        ;jump to setup...\r\n0

;clear zero
times 510 - ($ - $$) db 0
dw 0xaa55