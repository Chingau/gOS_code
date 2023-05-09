/*
 * create by gaoxu on 2023.05.08
 * */
#ifndef __GOS_OSKERNEL_IO_H__
#define __GOS_OSKERNEL_IO_H__

char read_byte(int port);
short read_word(int port);

void write_byte(int port, int value);
void write_word(int port, int value);

#endif
