#include "fs.h"
#include "ide.h"
#include "global.h"
#include "inode.h"
#include "dir.h"
#include "kernel.h"
#include "string.h"
#include "debug.h"
#include "file.h"
#include "system.h"
#include "thread.h"

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
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;    //有多少个空闲块就需要有多少个bit来管理
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

    //超始块初始化
    struct super_block sb;
    sb.magic = 0x20230904;
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
        return root_dir.inode->i_no;    // 0
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
                    if (sb_buf->magic == 0x20230904) {
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
    //对目录要用dir_open，这里只能open文件
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
    } else if (found && (flags & O_CREAT)) {  //若要创建的文件已存在
        printk("%s has already exist.\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    switch (flags & O_CREAT) {
        case O_CREAT:
            //printk("creating file.\n");
            fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1), flags);
            dir_close(searched_record.parent_dir);
        break;

        //其余为打开已存在的文件
        default:
            fd = file_open(inode_no, flags);
        break;
    }

    //此fd是指任务pcb->fd_table数组中的元素下标，并不是指全局file_table中的下标
    return fd;
}

static uint32_t fd_local2global(uint32_t local_fd)
{
    struct task_struct *curr = running_thread();
    int32_t global_fd = curr->fd_table[local_fd];
    ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
    return (uint32_t)global_fd;
}

/* 关闭文件描述符fd指向的文件，成功返回0，失败返回-1 */
int32_t sys_close(int32_t fd)
{
    int32_t ret = -1;
    if (fd > 2) {
        uint32_t g_fd = fd_local2global(fd);
        ret = file_close(&file_table[g_fd]);
        running_thread()->fd_table[fd] = -1;    //使该文件描述符位置可用
    }
    return ret;
}

/*
    将buf中连续count个字节写入文件描述符fd
    成功则返回写入的字节数，失败返回-1
*/
int32_t sys_write(int32_t fd, const void *buf, uint32_t count)
{
    if (fd < 0) {
        printk("sys_write: fd error.\n");
        return -1;
    }

    if (fd == stdout_no) {
        char tmp_buf[1024] = {0};
        memcpy(tmp_buf, buf, count);
        printk("%s", tmp_buf);
        return count;
    }

    uint32_t g_fd = fd_local2global(fd);
    struct file *wr_file = &file_table[g_fd];
    if ((wr_file->fd_flag & O_WRONLY) || (wr_file->fd_flag & O_RDWR)) {
        uint32_t bytes_written = file_write(wr_file, buf, count);
        return bytes_written;
    } else {
        printk("sys_write: not allowed to write file without flag O_RDWR or O_WRONLY,\n");
        return -1;
    }
}

/*
    从文件描述符fd指向的文件中读取count个字节到buf,
    若成功则返回读出的字节数，到文件尾则返回-1
*/
int32_t sys_read(int32_t fd, void *buf, uint32_t count)
{
    if (fd < 0) {
        printk("%s[%d]: fd error.\n", __FUNCTION__, __LINE__);
        return -1;
    }
    ASSERT(buf != NULL);
    uint32_t g_fd = fd_local2global(fd);
    return file_read(&file_table[g_fd], buf, count);
}

/*
    重置用于文件读写操作的偏移指针
    成功时返回新的偏移量，出错时返回-1
*/
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence)
{
    if (fd < 0) {
        printk("%s[%d]: fd error.\n", __FUNCTION__, __LINE__);
        return -1;
    }
    ASSERT(whence > 0 && whence < 4);
    uint32_t g_fd = fd_local2global(fd);
    struct file *pf = &file_table[g_fd];
    int32_t new_pos = 0;        //新的偏移量必须位于文件大小之内
    int32_t file_size = (int32_t)pf->fd_inode->i_size;

    switch (whence) {
        case SEEK_SET:  //offset的参照物是文件开始处，也就是将读写指针设置为距文件开头偏移offset个字节处
            new_pos = offset;
        break;

        case SEEK_CUR:  //offset的参照物是当前读写位置，也就是将读写位置指针设置为当前位置+offset
            new_pos = (int32_t)pf->fd_pos + offset;
        break;

        case SEEK_END:  //offset的参照物是文件尺寸大小，即文件最后一个字节的下一个字节，也就是将读写位置指针设置为文件尺寸+offset
            new_pos = file_size + offset;
        break;
    }

    if (new_pos < 0 || new_pos > (file_size - 1)) {
        return -1;
    }
    pf->fd_pos = new_pos;
    return pf->fd_pos;
}

/* 删除文件(非目录),成功返回0，失败返回-1 */
int32_t sys_unlink(const char *pathname)
{
    ASSERT(strlen(pathname) < MAX_PATH_LEN);
    //先检查待删除的文件是否存在
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(pathname, &searched_record);
    ASSERT(inode_no != 0);
    if (inode_no == -1) {
        printk("%s[%d]: file %s not fount.\n", __FUNCTION__, __LINE__, pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }
    if (searched_record.file_type == FT_DIRECTORY) {
        printk("%s[%d]: can't delete a directory with unlink().\n", __FUNCTION__, __LINE__);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    //检查是否在已打开文件列表(文件表)中
    uint32_t file_idx = 0;
    while (file_idx < MAX_FILE_OPEN) {
        if (file_table[file_idx].fd_inode != NULL && (uint32_t)inode_no == file_table[file_idx].fd_inode->i_no) {
            break;
        }
        file_idx++;
    }
    if (file_idx < MAX_FILE_OPEN) {
        dir_close(searched_record.parent_dir);
        printk("%s[%d]: file %s is in use, not allow to delete.\n", __FUNCTION__, __LINE__, pathname);
        return -1;
    }
    ASSERT(file_idx == MAX_FILE_OPEN);

    //为delete_dir_entry申请缓冲区
    void *io_buf = sys_malloc(SECTOR_SIZE + SECTOR_SIZE);
    if (io_buf == NULL) {
        dir_close(searched_record.parent_dir);
        printk("%s[%d]: malloc for io_buf failed.\n", __FUNCTION__, __LINE__);
        return -1;
    }

    struct dir *parent_dir = searched_record.parent_dir;
    delete_dir_entry(curr_part, parent_dir, inode_no, io_buf);
    inode_release(curr_part, inode_no);
    sys_free(io_buf);
    dir_close(searched_record.parent_dir);
    return 0;
}

/* 创建目录pathname，成功返回0，失败返回-1 */
int32_t sys_mkdir(const char *pathname)
{
    /*
        创建目录所涉及的工作包括：
        1.确认待创建的新目录在文件系统上不存在
        2.为新目录创建inode
        3.为新目录分配1个块存储该目录中的目录项
        4.在新目录中创建两个目录项"."".."
        5.在新目录的父目录中添加新目录的目录项
        6.将以上资源的变更同步到硬盘
    */
    uint8_t rollback_step = 0;  //用于操作失败时回滚各资源状态
    void *io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (io_buf == NULL) {
        printk("%s[%d]:sys_malloc for io_buf failed.\n", __FUNCTION__, __LINE__);
        return -1;
    }
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = -1;
    inode_no = search_file(pathname, &searched_record);
    if (inode_no != -1) {       //如果找到了同名目录或文件，失败返回
        printk("%s[%d]:file or directory %s exist.\n", __FUNCTION__, __LINE__, pathname);
        rollback_step = 1;
        goto rollback;
    } else {
        //若未找到，也要判断是在最终目录没找到，还是某个中间目录不存在
        uint32_t pathname_depth = path_depth_cnt((char *)pathname);
        uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
        if (pathname_depth != path_searched_depth) {
            printk("%s[%d]:cannot access %s: Not a directory, subpath %s is't exist.\n", __FUNCTION__, __LINE__, pathname, searched_record.searched_path);
            rollback_step = 1;
            goto rollback;
        }
    }

    struct dir *parent_dir = searched_record.parent_dir;
    //目录名称后可能会有字符'/'，所以最好直接用searched_record.searched_path,无'/'
    char *dirname = strrchr(searched_record.searched_path, '/') + 1;
    inode_no = inode_bitmap_alloc(curr_part);
    if (inode_no == -1) {
        printk("%s[%d]:allocate inode failed.\n", __FUNCTION__, __LINE__);
        rollback_step = 1;
        goto rollback;
    }

    struct inode new_dir_inode;
    inode_init(inode_no, &new_dir_inode);   //初始化inode

    uint32_t block_bitmap_idx = 0;
    int32_t block_lba = -1;
    block_lba = block_bitmap_alloc(curr_part);  //为目录分配一个块，用来写入目录.和..
    if (block_lba == -1) {
        printk("%s[%d]:block_bitmap_alloc for create directory failed.\n", __FUNCTION__, __LINE__);
        rollback_step = 2;
        goto rollback;
    }

    new_dir_inode.i_sectors[0] = block_lba;
    //每分配一个块就立即把位图同步到硬盘
    block_bitmap_idx = block_lba - curr_part->sb->data_start_lba;
    ASSERT(block_bitmap_idx != 0);
    bitmap_sync(curr_part, block_bitmap_idx, BLOCK_BITMAP);

    //将当前目录和目录项.和..写入目录
    memset(io_buf, 0, SECTOR_SIZE * 2);
    struct dir_entry *p_de = (struct dir_entry *)io_buf;

    memcpy(p_de->filename, ".", 1);
    p_de->i_no = inode_no;
    p_de->f_type = FT_DIRECTORY;

    p_de++;
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = parent_dir->inode->i_no;
    p_de->f_type = FT_DIRECTORY;
    ide_write(curr_part->my_disk, new_dir_inode.i_sectors[0], io_buf, 1);

    new_dir_inode.i_size = 2 * curr_part->sb->dir_entry_size;

    //在父目录中添加自己的目录项
    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));
    create_dir_entry(dirname, inode_no, FT_DIRECTORY, &new_dir_entry);
    memset(io_buf, 0, SECTOR_SIZE * 2);
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
        //sync_dir_entry中将block_bitmap通过bitmap_sync同步到硬盘
        printk("%s[%d]:sync_dir_entry to disk failed.\n");
        rollback_step = 2;
        goto rollback;
    }

    //父目录的inode同步到硬盘
    memset(io_buf, 0, SECTOR_SIZE * 2);
    inode_sync(curr_part, parent_dir->inode, io_buf);

    //将新创建目录的inode同步到硬盘
    memset(io_buf, 0, SECTOR_SIZE * 2);
    inode_sync(curr_part, &new_dir_inode, io_buf);

    //将inode位图同步到硬盘
    bitmap_sync(curr_part, inode_no, INODE_BITMAP);
    sys_free(io_buf);

    //关闭所创建目录的父目录
    dir_close(searched_record.parent_dir);
    return 0;

rollback:
    switch (rollback_step) {
        case 2:
            bitmap_set(&curr_part->inode_bitmap, inode_no, 0);
        case 1:
            dir_close(searched_record.parent_dir);
        break;
    }
    sys_free(io_buf);
    return -1;
}

/* 目录打开成功后返回目录指针，失败返回NULL */
struct dir *sys_opendir(const char *name)
{
    ASSERT(strlen(name) < MAX_PATH_LEN);
    /* 如果是根目录'/'，直接返回&root_dir */
    if (name[0] == '/' && (name[1] == 0 || name[1] == '.'))
        return &root_dir;

    //先检查待打开的目录是否存在
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(name, &searched_record);
    struct dir *ret = NULL;
    if (inode_no == -1) {
        printk("%s[%d]:In %s, sub path %s not exist.\n", __FUNCTION__, __LINE__, name, searched_record.searched_path);
    } else {
        if (searched_record.file_type == FT_REGULAR) {
            printk("%s[%d]:%s is regular file.\n", __FUNCTION__, __LINE__, name);
        } else if (searched_record.file_type == FT_DIRECTORY) {
            ret = dir_open(curr_part, inode_no);
        }
    }
    dir_close(searched_record.parent_dir);
    return ret;
}

/* 关闭目录，成功关闭目录p_dir并返回0，失败返回-1 */
int32_t sys_closedir(struct dir *dir)
{
    int32_t ret = -1;
    if (dir != NULL) {
        dir_close(dir);
        ret = 0;
    }
    return ret;
}

/* 读取目录dir的1个目录项，成功后返回其目录项地址，到目录尾时或出错时返回NULL */
struct dir_entry *sys_readdir(struct dir *dir)
{
    ASSERT(dir != NULL);
    return dir_read(dir);
}

/* 把目录dir的指针dir_pos置0, 实现目录回绕功能 */
void sys_rewinddir(struct dir *dir)
{
    dir->dir_pos = 0;
}

/* 删除空目录，成功时返回0，失败时返回-1 */
int32_t sys_rmdir(const char *pathname)
{
    //先检查待删除的文件是否存在
    struct path_search_record searched_record;
    int retval = -1;

    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(pathname, &searched_record);
    ASSERT(inode_no != 0);
    if (inode_no == -1) {
        printk("%s[%d]:In %s, sub path %s not exist.\n", __FUNCTION__, __LINE__, pathname, searched_record.searched_path);
    } else {
        if (searched_record.file_type == FT_REGULAR) {
            printk("%s[%d]:%s is regular file.\n", __FUNCTION__, __LINE__, pathname);
        } else {
            struct dir *dir = dir_open(curr_part, inode_no);
            if (!dir_is_empty(dir)) {  //非空目录不可删除
                printk("%s[%d]:dir %s is not empty, it is not allowed to delete a nonempty directory.\n", __FUNCTION__, __LINE__, pathname);
            } else {
                if (dir_remove(searched_record.parent_dir, dir) == 0) {
                    retval = 0;
                }
            }
            dir_close(dir);
        }
    }
    dir_close(searched_record.parent_dir);
    return retval;
}

/* 获得父目录的inode编号 */
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr, void *io_buf)
{
    struct inode *child_dir_inode = inode_open(curr_part, child_inode_nr);
    uint32_t block_lba = child_dir_inode->i_sectors[0]; //目录中的目录项..中包括父目录inode编号，..位于目录的第0块
    ASSERT(block_lba >= curr_part->sb->data_start_lba);

    inode_close(child_dir_inode);
    ide_read(curr_part->my_disk, block_lba, io_buf, 1);

    struct dir_entry *dir_e = (struct dir_entry *)io_buf;
    ASSERT(dir_e[1].i_no < MAX_FILES_PER_PART && dir_e[1].f_type == FT_DIRECTORY);    //第0个目录项是. 第1个目录项是..
    return dir_e[1].i_no;
}

/*
 在inode编号为p_inode_nr的目录中查找
 inode编号为c_inode_nr的子目录的名字，
 将名字存入缓冲区path，成功返回0，失败返回-1
*/
static int get_child_dir_name(uint32_t p_inode_nr, uint32_t c_inode_nr, char *path, void *io_buf)
{
    struct inode *parent_dir_inode = inode_open(curr_part, p_inode_nr);
    uint8_t block_idx = 0;
    //填充all_blocks，将该目录的所占扇区地址全部写入all_blocks
    uint32_t all_blocks[140] = {0}, block_cnt = 12;
    
    while (block_idx < 12) {
        all_blocks[block_idx] = parent_dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    if (parent_dir_inode->i_sectors[12]) {
        //若包含了一级间接块表，将其读入all_blocks
        ide_read(curr_part->my_disk, parent_dir_inode->i_sectors[12], all_blocks + 12, 1);
        block_cnt = 140;
    }
    inode_close(parent_dir_inode);

    struct dir_entry *dir_e = (struct dir_entry *)io_buf;
    uint32_t dir_entry_size = curr_part->sb->dir_entry_size;
    uint32_t dir_entrys_per_sec = (BLOCK_SIZE / dir_entry_size);

    block_idx = 0;
    while (block_idx < block_cnt) {
        if (all_blocks[block_idx]) {    //如果块不为空，则读入相应块
            ide_read(curr_part->my_disk, all_blocks[block_idx], io_buf, 1);
            uint8_t dir_e_idx = 0;
            //遍历每个目录项
            while (dir_e_idx < dir_entrys_per_sec) {
                if ((dir_e + dir_e_idx)->i_no == c_inode_nr) {
                    strcat(path, "/");
                    strcat(path, (dir_e + dir_e_idx)->filename);
                    return 0;
                }
                dir_e_idx++;
            }
        }
        block_idx++;
    }
    return -1;
}

/*
 把当前工作目录绝对路径写入buf, size是buf的大小，当buf为NULL时，
 由操作系统分配存储工作路径的空间并返回地址，
 失败则返回NULL
*/
char *sys_getcwd(char *buf, uint32_t size)
{
    /*
    确保buf不为空，若用户进程提供的buf为NULL，
    系统调用getcwd中要为用户进程通过malloc分配内存
    */
    ASSERT(buf != NULL);
    void *io_buf = sys_malloc(SECTOR_SIZE);
    if (io_buf == NULL) {
        return NULL;
    }

    struct task_struct *curr_thread = running_thread();
    int32_t parent_inode_nr = 0;
    int32_t child_inode_nr = curr_thread->cwd_inode_nr;
    ASSERT(child_inode_nr >= 0 && child_inode_nr < MAX_FILES_PER_PART);

    //若当前目录是根目录，直接返回 /
    if (child_inode_nr == 0) {
        buf[0] = '/';
        buf[1] = 0;
        return buf;
    }

    memset(buf, 0, size);
    char full_path_reverse[MAX_PATH_LEN] = {0}; //用来做全路径缓冲区

    /*
     从下往上逐层找父目录，直到找到根目录为止
     当child_inode_nr为根目录的inode编号0时停止
     即已经查看完根目录中的目录项
    */
    while (child_inode_nr) {
        parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr, io_buf);
        if (get_child_dir_name(parent_inode_nr, child_inode_nr, full_path_reverse, io_buf) == -1) {
            sys_free(io_buf);   //未找到名字，失败退出
            return NULL;
        }
        child_inode_nr = parent_inode_nr;
    }
    ASSERT(strlen(full_path_reverse) <= size);

    /*
     至此full_path_reverse中的路径是反着的，
     即子目录在前(左),父目录在后(右)
     现将其路径反置
    */
    char *last_slash;     //用于记录字符串中最后一个斜杠地址
    while ((last_slash = strrchr(full_path_reverse, '/'))) {
        uint16_t len = strlen(buf);
        strcpy(buf + len, last_slash);
        //在full_path_reverse中添加结束字符，作为下一次执行strcpy中last_slash的边界
        *last_slash = 0;
    }
    sys_free(io_buf);
    return buf;
}

/* 更改当前工作目录为绝对路径path，成功则返回0， 失败返回-1 */
int32_t sys_chdir(const char *path)
{
    int32_t ret = -1;
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(path, &searched_record);
    if (inode_no != -1) {
        if (searched_record.file_type == FT_DIRECTORY) {
            running_thread()->cwd_inode_nr = inode_no;
            ret = 0;
        } else {
            printk("%s[%d]:%s is regular file or other.\n", __FUNCTION__, __LINE__, path);
        }
    }
    dir_close(searched_record.parent_dir);
    return ret;
}