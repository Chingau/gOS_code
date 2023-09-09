; 0柱面0磁道2扇区
section loader vstart=0x500 ;把下面的代码放到0x500开始的地址处
KERNEL_BASE_ADDR equ 0x1200
STACK_TOP_ADDR equ 0x9f000
ARDS_BUFFER equ 0x1102
ARDS_NR_BUFFER equ 0x1100   ;配合C语言结构体部分

TI_GDT equ 000b
TI_LDT equ 100b
RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
PRL3 equ 11b

SELECTOR_CODE equ (0x0001 << 3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002 << 3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

;构建GDT表
GDT_BASE:
    db 0, 0, 0, 0, 0, 0, 0, 0
GDT_CODE:
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x98, 0xCF, 0x00
GDT_DATA:
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00
GDT_B8000:
    ;段基址为0xB8000，段界限为0x7FFF，粒度为1B，非系统段(数据段，只读)
    db 0xFF, 0x7F, 0x00, 0x80, 0x0B, 0x92, 0x40, 0x00

GDT_SIZE equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1

    times 60 dq 0       ;此处预留60个描述符的空位

;gdt指针，前2字节是gdt界限，后4字节是gdt起始地址
gdt_ptr:
    dw GDT_LIMIT
    dd GDT_BASE

;以下两个值定义在GDT表后面，因为它们是占空间的，这样GDT表就是[0x500]这个地址处
ARDS_NR dw 0
CHECK_BUFFER_OFFER dw 0

[BITS 16]
global setup_start
setup_start:
    mov     ax, 0
    mov     ss, ax
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     si, ax

    mov si, prepare_enter_protected_mode_msg
    call print
    ;xchg bx, bx

;------------------------------------------------;
; 内存检测
;------------------------------------------------;
memory_check:
    xor ebx, ebx
    mov di, ARDS_BUFFER     ;es:di指向一块内存，用于保存读出来的ARDS结构; es为0,前面已赋值为0
    mov edx, 0x534d4150
.loop:
    mov eax, 0xe820
    mov ecx, 20
    int 0x15
    jc memory_check_error   ;if(CF!=0),内存检测出错
    add di, cx              ;将下次读到的ARDS结构保存到下一个内在地址处
    inc word [ARDS_NR]      ;记录有多少个ARDS结构
    cmp ebx, 0              ;在检测的时候，ebx会被bios修改，ebx不等于0时需要继续检测
    jne .loop
    mov ax, [ARDS_NR]       ;保存ARDS结构的个数
    mov [ARDS_NR_BUFFER], ax
    mov [CHECK_BUFFER_OFFER], di    ;保存offset
.memory_check_success:
    mov si, memory_check_success_msg
    call print

;进入保护模式
;   1.打开A20
;   2.加载gdt
;   3.将cr0的pe位置1
enter_protected_mode:
    ;关中断
    cli
    ; 打开A20
    in al, 0x92
    or al, 0000_0010b
    out 0x92, al
    ; 加载gdt
    lgdt [gdt_ptr]
    ; cr0第0位置1
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    ; 刷新流水线
    jmp SELECTOR_CODE:p_mode_start

;------------------------------------------------;
; 内存检测出错信息打印
;------------------------------------------------;
memory_check_error:
    mov si, memory_check_error_msg
    call print
    jmp $

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


[BITS 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov ax, SELECTOR_VIDEO
    mov gs, ax
    mov esp, STACK_TOP_ADDR

    mov ecx, 3  ;从硬盘的第3个扇区开始读
    mov bl, 254  ;共读取256个扇区
    call read_hd

    ;xchg bx, bx
    jmp SELECTOR_CODE:KERNEL_BASE_ADDR

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
    mov edi, KERNEL_BASE_ADDR    ;从硬盘读取到的数据存放在内在的起始地址处
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

prepare_enter_protected_mode_msg:
    db "Prepare to go into protected mode...", 10, 13, 0

memory_check_error_msg:
    db "memory check fail...", 10, 13, 0

memory_check_success_msg:
    db "memory check success...", 10, 13, 0