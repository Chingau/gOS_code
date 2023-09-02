#ifndef __GOS_OSKERNEL_IDE_H__
#define __GOS_OSKERNEL_IDE_H__
#include "types.h"
#include "bitmap.h"
#include "list.h"
#include "sync.h"
#include "super_block.h"

/* 分区表结构 */
struct partition {
    uint32_t start_lba;         //起始扇区
    uint32_t sec_cnt;           //扇区数
    struct disk *my_disk;       //分区所属的硬盘
    struct list_elem part_tag;  //用于队列中的标记
    char name[8];               //分区名称
    struct super_block *sb;     //本分区的超级块
    bitmap_t block_bitmap;      //块位图
    bitmap_t inode_bitmap;      //inode位图
    struct list open_inodes;    //本分区打开的inode队列
};

/* 硬盘结构 */
struct disk {
    char name[16];                      //本硬盘的名称
    struct ide_channel *my_channel;     //此块硬盘归属于哪个ide通道
    uint8_t dev_no;                     //本硬盘是主0，还是从1
    struct partition prim_parts[4];     //主分区最多是4个
    struct partition logic_parts[8];    //逻辑分区数量无限，但总得有个支持的上限，这里支持8个
};

/* ata通道结构 */
struct ide_channel {
    char name[8];               //本ata通道名称
    uint16_t port_base;         //本通道的起始端口号
    uint8_t irq_no;             //本通道所用的中断号
    lock_t lock;                //通道锁
    bool expecting_intr;        //表示等待硬盘的中断
    semaphore_t disk_done;      //用于阻塞、唤醒驱动程序
    struct disk devices[2];     //一个通道上连接两个硬盘，一主一从
};

extern uint8_t channel_cnt;
extern struct ide_channel channels[2];

void ide_init(void);
void ide_read(struct disk *hd, uint32_t lba, void *buf, uint32_t sec_cnt);
void ide_write(struct disk *hd, uint32_t lba, void *buf, uint32_t sec_cnt);
#endif
