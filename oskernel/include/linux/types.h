/*
 * create by gaoxu on 2023.05.10
 * 该文件主要为一些常用的变量类型定义别名
 * */

#ifndef __GOS_OSKERNEL_TYPES_H__
#define __GOS_OSKERNEL_TYPES_H__

#define EOF -1                      //end of file
#define NULL ((void *)0)             //空指针
#define EOS '\0'        //字符串结尾
#define bool _Bool
#define true 1
#define false 0
typedef unsigned int size_t;
typedef unsigned int ssize_t;
typedef long long int64;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

typedef int int32_t;
typedef short int16_t;
typedef char int8_t;

typedef int16_t pid_t;

#endif
