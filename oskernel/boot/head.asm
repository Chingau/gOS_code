[SECTION .data]
;页目录项起始位置
PAGE_DIR_TABLE_POS equ 0x100000
;------页表相关属性
PG_P        equ 1b
PG_RW_R     equ 00b
PG_RW_W     equ 10b
PG_US_S     equ 000b
PG_US_U     equ 100b

[SECTION .text]
[BITS 32]
extern kernel_main
extern gdt_ptr
global _start

_start:
    call config_8259a
    call setup_page
    call turn_on_page
    call enter_c_world

;配置8259a芯片，响应中断用
config_8259a:
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
    mov al, 1111_1101b
    out 21h, al

    ;向从片发送OCW1，屏蔽从片所有中断响应
.disable_8259a_slave:
    mov al, 1111_1111b
    out 0a1h, al
    ret

;创建页目录及页表
setup_page:
    ;先把页目录占用的空间逐字节清0
    mov ecx, 4096
    mov esi, 0
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir
;创建页目录项PDE
.create_pde:
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000
    mov ebx, eax
    or eax, PG_US_U | PG_RW_W | PG_P
    ;写入页目录表中第0项，即往地址0x100000处写入0x101007(其中的7是属性)
    mov [PAGE_DIR_TABLE_POS + 0x0], eax
    ;写入页目录表中第768项，即往地址0x100c00处写入0x101007
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax
    sub eax, 0x1000
    ;页目录表的最后一项写入页目录表自身地址，即往地址0x100ffc处写入0x100007
    mov [PAGE_DIR_TABLE_POS + 4092], eax

;创建页表项PTE
    mov ecx, 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
    ;第一个页表的前256页线性的映射到低端1MB物理内存中
.create_pte:
    mov [ebx+esi*4], edx
    add edx, 4096
    inc esi
    loop .create_pte
    
;创建内核其它页目录项PDE
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000
    or eax, PG_US_U | PG_RW_W | PG_P
    mov ebx, PAGE_DIR_TABLE_POS
    ;填充页目录表中的第769～1022的所有页目录项
    mov ecx, 254
    mov esi, 769
.create_kernel_pde:
    mov [ebx+esi*4], eax
    inc esi
    add eax, 0x1000
    loop .create_kernel_pde
    ret

;开启分页机制
turn_on_page:
    ;先保存未开启分页机制下的gdt表
    sgdt [gdt_ptr]
    ;获取到gdt表在内存中的起始地址，低2字节是GDT界限，所以要加2
    mov ebx, [gdt_ptr + 2]

    ;GDT的第0项为全0,跳过不修改，所以esi从1开始
    mov esi, 1
    mov cx, 3
.modify_gdt_item_base:
    ;修改GDT中每项的段基址，即从0xc000000开始
    ;esi*8代表GDT表中的每一项占8B
    ;+4代表偏移到GDT表中每项的高4个字节
    or dword [ebx + esi*8 + 4], 0xc0000000
    inc esi
    loop .modify_gdt_item_base

    ;修改gdt在内存中的起始地址
    add dword [gdt_ptr + 2], 0xc0000000
    ;记得同时修改栈顶指针
    add esp, 0xc0000000

    ;设置页目录表基地址
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax
    ;设置cr0的最高位为1开启分页机制
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    lgdt [gdt_ptr]
    ret

enter_c_world:
    xchg bx, bx
    call kernel_main
    jmp $