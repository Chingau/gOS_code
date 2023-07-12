#ifndef __GOS_OSKERNEL_IO_H_
#define __GOS_OSKERNEL_IO_H_
#include "types.h"

/* 向端口port写入一个字节 */
static inline void outb(uint16_t port, uint8_t data)
{
    /* b和w代表位宽，%b0代表al,%w1代表dx； N为立即数约束，它表示0~255的立即数，约束port取值范围*/
    __asm__ volatile("outb %w1, %b0" : : "a"(data), "Nd"(port));
    /*
        mov al, data
        mov dx, port
        outb dx, al
    */
}

/* 将addr处起始的word_cnt个字写入到端口port中 */
static inline void outsw(uint16_t port, const void *addr, uint32_t word_cnt)
{
    /*
     源地址: ds:[esi]
     目的地址: es:[edi]
     复制长度: ecx
     + 表示此限制即作输入又作输出
     注意，outsw与movsw不同：movsw会同时更新esi和edi，而outsw只会更新esi
     另：在初始化的时候cs,ds,es,ss都已经设置为0了
    */
    __asm__ volatile("cld; rep outsw" : "+S"(addr), "+c"(word_cnt) : "d"(port));
    /*
        mov esi, addr
        mov ecx, word_cnt
        mov dx, port
        cld
        rep outsw
    */
}

/* 从port端口中读出一个字节并返回 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t data;
    __asm__ volatile("inb %b0, %w1" : "=a"(data) : "Nd"(port));
    return data;
    /*
        mov dx, port
        inb al, dx
        mov data, al
    */
}

/* 从端口port中读取word_cnt个字保存到内存addr地址起始处 */
static inline void insw(uint16_t port, void *addr, uint32_t word_cnt)
{
    __asm__ volatile("cld; rep insw" : "+D"(addr), "+c"(word_cnt) : "d"(port) : "memory");
    /*
        mov edi, addr
        mov ecx, word_cnt
        mov dx, port
        cld
        rep insw
    */
}

#endif
