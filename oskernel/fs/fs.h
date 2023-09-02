#ifndef __GOS_FS_H__
#define __GOS_FS_H__

#define MAX_FILES_PER_PART  4096            //每个分区所支持最大创建的文件数
#define SECTOR_SIZE 512                     //扇区字节大小
#define BITS_PER_SECTOR (SECTOR_SIZE * 8)   //每扇区的bit数
#define BLOCK_SIZE  SECTOR_SIZE             //块字节大小(这里定义一个块就等于一个扇区)

/* 文件类型 */
enum file_types {
    FT_UNKNOWN,         //不支持的文件类型
    FT_REGULAR,         //普通文件
    FT_DIRECTORY        //目录文件
};

void filesys_init(void);

#endif