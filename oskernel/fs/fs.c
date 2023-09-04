#include "fs.h"
#include "ide.h"
#include "global.h"
#include "inode.h"
#include "dir.h"
#include "kernel.h"
#include "string.h"
#include "debug.h"
#include "file.h"

struct partition *curr_part; //默认情况下操作的是哪个分区 

/* 在分区链表中找到名为part_name的分区，并将其指针赋值给curr_part */
static bool mount_partition(struct list_elem *pelem, int arg)
{
    char *part_name = (char *)arg;
    struct partition *part = elem2entry(struct partition, part_tag, pelem);

    if (!strcmp(part->name, part_name)) {
        curr_part = part;
        struct disk *hd = curr_part->my_disk;

        /* sb_buf用来存储从硬盘上读入的超级块 */
        struct super_block *sb_buf = (struct super_block *)sys_malloc(SECTOR_SIZE);
        if (sb_buf == NULL)
            PANIC("alloc memory failed!");
        
        /* 读入超级块 */
        ide_read(hd, curr_part->start_lba + 1, sb_buf, 1);
        curr_part->sb = sb_buf;

        /* 将硬盘上的块位图读入到内存 */
        curr_part->block_bitmap.bits = (uint8_t *)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
        if (curr_part->block_bitmap.bits == NULL)
            PANIC("alloc memory failed!");
        curr_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;

        /* 从硬盘上读入块位图到分区的block_bitmap.bits */
        ide_read(hd, sb_buf->block_bitmap_lba, curr_part->block_bitmap.bits, sb_buf->block_bitmap_sects);

        /* 将硬盘上的inode位图读入到内存 */
        curr_part->inode_bitmap.bits = (uint8_t *)sys_malloc(sb_buf->inode_bitmap_secs * SECTOR_SIZE);
        if (curr_part->inode_bitmap.bits == NULL)
            PANIC("alloc memory failed!");
        curr_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_secs * SECTOR_SIZE;

        /* 从硬盘上读入inode位图到分区的inode_bitmap.bits */
        ide_read(hd, sb_buf->inode_bitmap_lba, curr_part->inode_bitmap.bits, sb_buf->inode_bitmap_secs);

        list_init(&curr_part->open_inodes);
        printk("mount \"%s\" done.\n", part->name);
        return true;
    }
    return false;
}

/* 格式化分区，也就是初始化分区的元信息，创建文件系统 */
static void partition_format(struct partition *part)
{
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;
    //inode位图占用的扇区数，最多支持4096个文件
    uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
    //inode_table占用的扇区数
    uint32_t inode_table_sects = DIV_ROUND_UP((sizeof(struct inode) * MAX_FILES_PER_PART), SECTOR_SIZE);

    uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
    uint32_t free_sects = part->sec_cnt - used_sects;

    //块位图占据的扇区数
    uint32_t block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

    //超始块初始化
    struct super_block sb;
    sb.magic = 0x20230902;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;

    sb.block_bitmap_lba = sb.part_lba_base + 2; //第0块是引导块，第1块是超始块
    sb.block_bitmap_sects = block_bitmap_sects;

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
    sb.inode_bitmap_secs = inode_bitmap_sects;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_secs;
    sb.inode_table_sects = inode_table_sects;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
    sb.root_inode_no = 0;       //根目录的inode编号从0开始，即inode数组中第0个inode是根目录
    sb.dir_entry_size = sizeof(struct dir_entry);

    printk("\"%s\" info:\n", part->name);
    printk("    magic:0x%08x\n", sb.magic);
    printk("    part_lba_base:0x%x\n", sb.part_lba_base);
    printk("    all_sectors:0x%x\n", sb.sec_cnt);
    printk("    inode_cnt:0x%x\n", sb.inode_cnt);
    printk("    block_bitmap_lba:0x%x\n", sb.block_bitmap_lba);
    printk("    block_bitmap_sectors:0x%x\n", sb.block_bitmap_sects);
    printk("    inode_bitmap_lba:0x%x\n", sb.inode_bitmap_lba);
    printk("    inode_bitmap_secs:0x%x\n", sb.inode_bitmap_secs);
    printk("    inode_table_lba:0x%x\n", sb.inode_table_lba);
    printk("    inode_table_sects:0x%x\n", sb.inode_table_sects);
    printk("    data_start_lba:0x%x\n", sb.data_start_lba);

    struct disk *hd = part->my_disk;
    /*
    (1)将超级块写入本分区的1扇区
    */
    ide_write(hd, part->start_lba + 1, &sb, 1);
    printk("    super_block_lba:0x%x\n", part->start_lba + 1);

    //找出数据量最大的元信息，用其尺寸做存储缓冲区
    uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_secs) ? sb.block_bitmap_sects : sb.inode_bitmap_secs;
    buf_size = (buf_size >= sb.inode_table_sects) ? buf_size : sb.inode_table_sects;
    buf_size *= SECTOR_SIZE;

    uint8_t *buf = (uint8_t *)sys_malloc(buf_size);

    /*
    (2)将块位图初始化并写入硬盘sb.block_bitmap_lba处
    */
    buf[0] |= 0x01;     //第0个块预留给根目录，位图中先占位
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
    //last_size是块位图所在最后一个扇区中，不足一扇区的其余部分
    uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);

        //1.先将位图最后一字节到其所在的扇区的结束全置为1，即超出实际块数的部分直接置为已占用
    memset(&buf[block_bitmap_last_byte], 0xff, last_size);
        //2.再将上一步中覆盖的最后一字节内的有效位重新置0
    uint8_t bit_idx = 0;
    while (bit_idx <= block_bitmap_last_bit)
        buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);

    /*
    (3)将inode位图初始化并写入硬盘sb.inode_bitmap_lba处
    */
    memset(buf, 0, buf_size);
    buf[0] |= 0x01;     //第0个inode分给了根目录
    /*
        由于inode_table中共4096个inode
        位图inode_bitmap正好占用1个扇区
        即inode_bitmap_sects等于1
        所以位图中的位全部都代表inode_table中的inode
        无需再像block_bitmap那样单独处理最后一扇区的剩余部分
        inode_bitmap所在的扇区中没有多余的无效位
    */
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_secs);

    /*
    (4)将inode数组初始化并写入硬盘sb.inode_table_lba处
    */
    memset(buf, 0, buf_size);
    struct inode *i = (struct inode *)buf;
    i->i_size = sb.dir_entry_size * 2;      // .和..
    i->i_no = 0;        //根目录占inode数组中第0个inode
    i->i_sectors[0] = sb.data_start_lba;
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

    /*
    (5)将根目录写入硬盘sb.data_start_lba处
    */
    memset(buf, 0, buf_size);
    struct dir_entry *p_de = (struct dir_entry *)buf;
    //初始化当前目录 .
    memcpy(p_de->filename, ".", 1);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    p_de++;

    //初始化当前目录的父目录 ..
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = 0;     //根目录的父目录依然是根目录自己
    p_de->f_type = FT_DIRECTORY;
    //sb.data_start_lba已经分配给了根目录，里面是根目录的目录项
    ide_write(hd, sb.data_start_lba, buf, 1);

    printk("    root_dir_lba:0x%x\n", sb.data_start_lba);
    sys_free(buf);
}

/* 将最上层路径名称解析出来 */
static char *path_parse(char *pathname, char *name_store)
{
    if (pathname[0] == '/') {    //根路径不需要单独解析
        //路径中出现1个或多个连续的字符‘/’则跳过
        while (*(++pathname) == '/');
    }

    //开始一般的路径解析
    while (*pathname != '/' && *pathname != 0) {
        *name_store++ = *pathname++;
    }

    if (pathname[0] == 0)   //若路径字符串为空，则返回NULL
        return NULL;
    
    return pathname;
}

/* 返回路径深度，比如 /a/b/c，深度为3 */
int32_t path_depth_cnt(char *pathname)
{
    ASSERT(pathname != NULL);
    char *p = pathname;
    char name[MAX_FILE_NAME_LEN] = {0};
    uint32_t depth = 0;

    p = path_parse(p, name);
    while (name[0]) {
        depth++;
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (p)
            p = path_parse(p, name);
    }
    return depth;
}

/* 搜索文件pathname,若找到则返回其inode编号，否则返回-1 */
static int search_file(const char *pathname, struct path_search_record *searched_record)
{
    //如果待查找的是根目录，为避免下面无用的查找，直接返回已知根目录信息
    if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")) {
        searched_record->parent_dir = &root_dir;
        searched_record->file_type = FT_DIRECTORY;
        searched_record->searched_path[0] = 0;
        return 0;
    }

    uint32_t path_len = strlen(pathname);
    ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
    char *sub_path = (char *)pathname;
    struct dir *parent_dir = &root_dir;
    struct dir_entry dir_e;
    //记录路径解析出来的各级名称，如路径"/a/b/c"，数组name每次的值分别为"a","b","c"
    char name[MAX_FILE_NAME_LEN] = {0};
    
    searched_record->parent_dir = parent_dir;
    searched_record->file_type = FT_UNKNOWN;
    uint32_t parent_inode_no = 0;   //父目录的inode号

    sub_path = path_parse(sub_path, name);
    while (name[0]) {
        ASSERT(strlen(searched_record->searched_path) < MAX_PATH_LEN);

        //记录已存在的父目录
        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name);
        
        //在所给的目录中查找文件
        if (search_dir_entry(curr_part, parent_dir, name, &dir_e)) {
            memset(name, 0, MAX_FILE_NAME_LEN);
            //若sub_path不等于NULL，也就是未结束时继续拆分路径
            if (sub_path) {
                sub_path = path_parse(sub_path, name);
            }

            if (dir_e.f_type == FT_DIRECTORY) {
                parent_inode_no = parent_dir->inode->i_no;
                dir_close(parent_dir);
                parent_dir = dir_open(curr_part, dir_e.i_no);    //更新父目录
                searched_record->parent_dir = parent_dir;
                continue;
            } else if (dir_e.f_type == FT_REGULAR) {
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no;
            }
        } else {
            //找不到目录项时，要留着parent_dir不要关闭，若是创建新文件的话需要在parent_dir中创建
            return -1;
        }
    }

    //已遍历完整个路径，并且查找的文件或目录只有同名目录存在
    dir_close(searched_record->parent_dir);

    //保存被查找目录的直接父目录
    searched_record->parent_dir = dir_open(curr_part, parent_inode_no);
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;
}

/* 在磁盘上搜索文件系统，若没有则格式化分区创建文件系统 */
void filesys_init(void)
{
    uint8_t channel_no = 0, dev_no, part_idx = 0;

    /* sb_buf用来存储从硬盘上读入的超级块 */
    struct super_block *sb_buf = (struct super_block *)sys_malloc(SECTOR_SIZE);
    if (sb_buf == NULL)
        PANIC("alloc memory failed!");
    
    printk("searching filesystem......\n");
    while (channel_no < channel_cnt) {
        dev_no = 0;
        while (dev_no < 2) {
            if (dev_no == 0) {      //跨过启动盘hd.img
                dev_no++;
                continue;
            }
            struct disk *hd = &channels[channel_no].devices[dev_no];
            struct partition *part = hd->prim_parts;
            while (part_idx < 12) {     //4个主分区+8个逻辑分区
                if (part_idx == 4) {
                    part = hd->logic_parts;     //开始处理逻辑分区
                }
                /*
                channels数组是全局变量，未初始化时默认为0，disk属于其嵌套结构，
                partition又为disk的嵌套结构，因此partition中的成员默认也为0
                */
                if (part->sec_cnt != 0) {   //如果分区存在
                    memset(sb_buf, 0, SECTOR_SIZE);
                    ide_read(hd, part->start_lba + 1, sb_buf, 1);   //从硬盘中读出超级块
                    if (sb_buf->magic == 0x20230902) {
                        printk("\"%s\" has filesystem.\n", part->name);
                    } else {
                        printk("formatting %s's partition %s......\n", hd->name, part->name);
                        partition_format(part);
                    }
                }
                part_idx++;
                part++; //下一分区
            }
            dev_no++;   //下一磁盘
        }
        channel_no++;   //下一通道
    }
    sys_free(sb_buf);

    char default_part[8] = "sdb1";      //默认操作的分区
    list_traversal(&partition_list, mount_partition, (int)default_part);

    //将当前分区的根目录打开
    open_root_dir(curr_part);

    //初始化文件表
    uint32_t fd_idx = 0;
    while (fd_idx < MAX_FILE_OPEN) {
        file_table[fd_idx++].fd_inode = NULL;
    }
}

/*
 打开或创建文件成功后，返回文件描述符，否则返回-1
*/
int32_t sys_open(const char *pathname, uint8_t flags)
{
    //对目录要用dir_open，这里只有open文件
    if (pathname[strlen(pathname) - 1] == '/') {
        printk("can't open a directory %s\n", pathname);
        return -1;
    }
    ASSERT(flags <= 7);
    int32_t fd = -1;
    struct path_search_record searched_record;
    
    memset(&searched_record, 0, sizeof(struct path_search_record));

    //记录目录深度，帮助判断中间某个不存在的情况 
    uint32_t pathname_depth = path_depth_cnt((char *)pathname);

    //先检查文件是否存在
    int inode_no = search_file(pathname, &searched_record);
    bool found = inode_no != -1 ? true : false;

    if (searched_record.file_type == FT_DIRECTORY) {
        printk("cna't open a directory with open(), use opendir() to instead.\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }

    uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
    //先判断是否把pathname的各层目录都访问到了，即是否在某个中间目录就失败了
    if (pathname_depth != path_searched_depth) {
        //说明并没有访问到全部的路径，某个中间目录是不存在的
        printk("cannot access %s: Not a directory, subpath %s is't exist.\n", pathname, searched_record.searched_path);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    //若是在最后一个路径上没找到，并且并不是要创建文件，直接返回-1
    if (!found && !(flags & O_CREAT)) {
        printk("in path %s, file %s is't exist.\n", searched_record.searched_path, (strrchr(searched_record.searched_path, '/') + 1));
        dir_close(searched_record.parent_dir);
        return -1;
    } else if (found && flags & O_CREAT) {  //若要创建的文件已存在
        printk("%s has already exist.\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    switch (flags & O_CREAT) {
        case O_CREAT:
            printk("creating file.\n");
            fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1), flags);
            dir_close(searched_record.parent_dir);
        break;
        //其余为打开文件
    }

    //此fd是指任务pcb->fd_table数组中的元素下标，并不是指全局file_table中的下标
    return fd;
}