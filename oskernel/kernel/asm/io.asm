[bits 32]
[SECTION .text]

global read_byte
;从端口读一字节
read_byte:
    push ebp
    mov ebp, esp
    xor eax, eax
    mov edx, [ebp + 8]  ;port
    in al, dx
    leave
    ret

global write_byte
;向端口写一字节
write_byte:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]  ;port
    mov eax, [ebp + 12] ;value
    out dx, al
    leave
    ret

global read_word
;从端口读一个字
read_word:
    push ebp
    mov ebp, esp
    xor eax, eax
    mov edx, [ebp + 8]  ;port
    in ax, dx
    leave
    ret

global write_word
;向端口写一个字
write_word:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]  ;port
    mov eax, [ebp + 12] ;value
    out dx, ax
    leave
    ret