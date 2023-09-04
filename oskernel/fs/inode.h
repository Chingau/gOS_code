#ifndef __GOS_OSKERNEL_INODE_H__
#define __GOS_OSKERNEL_INODE_H__
#include "types.h"
#include "list.h"
#include "ide.h"

/* inode结构 */
struct inode {
    uint32_t i_no;              //inode编号
    uint32_t i_size;            //当此inode是文件时，i_size是指文件大小;当此inode是目录时，i_size是指该目录下所有目录项大小之和
    uint32_t i_open_cnts;       //记录此文件被打开的次数
    bool write_deny;            //写文件不能并行，进程写文件前检查此标识
    uint32_t i_sectors[13];     //i_sectors[0~11]是直接块，i_sectors[12]用来存储一级间接块指针
    /*
     inode_tag用来加入"已打开的inode列表"，因为硬盘比较慢，为避免下次再打开该文件时还要从硬盘
     上重复载入inode，应该在该文件第一次被打开时就将其inode加入到内核缓存中，每次打开一个文件时，
     先在此缓存中查找相关inode，如果有就直接用，否则再从硬盘上读取inode
    */
    struct list_elem inode_tag;
};

void inode_sync(struct partition *part, struct inode *inode, void *io_buf);
struct inode *inode_open(struct partition *part, uint32_t inode_no);
void inode_close(struct inode *inode);
void inode_init(uint32_t inode_no, struct inode *new_inode);

#endif
