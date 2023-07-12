/*
 * create by gaoxu on 2023.05.10
 * 该文件主要是对终端的显示进行操作
 * */

#include "io.h"
#include "types.h"

#define CRT_ADDR_REG 0x3D4 // CRT(6845)索引寄存器
#define CRT_DATA_REG 0x3D5 // CRT(6845)数据寄存器

#define CRT_START_ADDR_H 0xC // 显示内存起始位置 - 高位
#define CRT_START_ADDR_L 0xD // 显示内存起始位置 - 低位
#define CRT_CURSOR_H 0xE     // 光标位置 - 高位
#define CRT_CURSOR_L 0xF     // 光标位置 - 低位

#define MEM_BASE 0xB8000              // 显卡内存起始位置
#define MEM_SIZE 0x4000               // 显卡内存大小
#define MEM_END (MEM_BASE + MEM_SIZE) // 显卡内存结束位置
#define WIDTH 80                      // 屏幕文本列数
#define HEIGHT 25                     // 屏幕文本行数
#define ROW_SIZE (WIDTH * 2)          // 每行字节数
#define SCR_SIZE (ROW_SIZE * HEIGHT)  // 屏幕字节数

#define ASCII_NUL 0x00
#define ASCII_ENQ 0x05
#define ASCII_BEL 0x07 // \a
#define ASCII_BS 0x08  // \b
#define ASCII_HT 0x09  // \t
#define ASCII_LF 0x0A  // \n
#define ASCII_VT 0x0B  // \v
#define ASCII_FF 0x0C  // \f
#define ASCII_CR 0x0D  // \r
#define ASCII_DEL 0x7F

static uint screen; // 当前显示器开始的内存位置
static uint pos; // 记录当前光标的内存位置
static uint x, y; // 当前光标的坐标

//设置当前显示器开始的位置
static void set_screen(void)
{
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);
    outb(CRT_DATA_REG, ((screen - MEM_BASE) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);
    outb(CRT_DATA_REG, ((screen - MEM_BASE) >> 1) & 0xff);
}

//设置光标的位置
static void set_cursor(void)
{
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    outb(CRT_DATA_REG, ((pos - MEM_BASE) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    outb(CRT_DATA_REG, ((pos - MEM_BASE) >> 1) & 0xff);
}

//清屏
static void console_clear(void)
{
    screen = MEM_BASE;
    pos = MEM_BASE;
    x = y = 0;
    set_cursor();
    set_screen();

    u16 *ptr = (u16 *)MEM_BASE;
    while (ptr < (u16 *)MEM_END) {
        *ptr++ = 0x0720;
    }
}

// 向上滚屏
static void scroll_up(void)
{
    char *dest, *src;
    size_t count;

    if (screen + SCR_SIZE + ROW_SIZE < MEM_END) {
        u32 *ptr = (u32 *)(screen + SCR_SIZE);
        for (size_t i = 0; i < WIDTH; i++) {
            *ptr++ = 0x0720;
        }
        screen += ROW_SIZE;
        pos += ROW_SIZE;
    } else {
        dest = (char *)MEM_BASE;
        src = (char *)screen;
        count = SCR_SIZE;
        while (count--) {
            *dest++ = *src++;
        }
        pos -= (screen - MEM_BASE);
        screen = MEM_BASE;
    }
    set_screen();
}

static void command_lf(void)
{
    if (y + 1 < HEIGHT) {
        y++;
        pos += ROW_SIZE;
        return;
    }
    scroll_up();
}

static void command_cr(void)
{
    pos -= (x << 1);
    x = 0;
}

static void command_bs(void)
{
    if (x) {
        x--;
        pos -= 2;
        *(u16 *)pos = 0x0720;
    }
}

static void command_del(void)
{
    *(u16 *)pos = 0x0720;
}

void console_write(char *buf, u32 count)
{
    char ch;
    char *ptr = (char *)pos;

    while (count--) {
        ch = *buf++;
        switch (ch) {
            case ASCII_NUL:
                break;
            case ASCII_BEL:
                break;
            case ASCII_BS:      //退格
                command_bs();
                break;
            case ASCII_HT:
                break;
            case ASCII_LF:      //换行
                command_lf();
                command_cr();
                break;
            case ASCII_VT:
                break;
            case ASCII_FF:      //换页
                command_lf();
                break;
            case ASCII_CR:      //回车
                command_cr();
                break;
            case ASCII_DEL:     //删除
                command_del();
                break;
            default:
                if (x >= WIDTH) {
                    x -= WIDTH;
                    pos -= ROW_SIZE;
                    command_lf();
                }

                *ptr = ch;
                ptr++;
                *ptr = 0x07;
                ptr++;

                pos += 2;
                x++;
                break;
        }
    }
    set_cursor();
}

void console_init(void)
{
    console_clear();
}
