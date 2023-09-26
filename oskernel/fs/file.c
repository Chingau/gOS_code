#include "file.h"
#include "thread.h"
#include "ide.h"
#include "fs.h"
#include "kernel.h"
#include "string.h"
#include "interrupt.h"
#include "debug.h"

/* 文件表 */
struct file file_table[MAX_FILE_OPEN];

/* 从文件表file_table中获取一个空闲位，成功返回下标，失败返回 -1 */
int32_t get_free_slot_in_global(void)
{
    uint32_t fd_idx = 3;

    while (fd_idx < MAX_FILE_OPEN) {
        if (file_table[fd_idx].fd_inode == NULL)
            break;
        fd_idx++;
    }

    if (fd_idx == MAX_FILE_OPEN) {
        printk("exceed max open files.\n");
        return -1;
    }

    return fd_idx;
}

/*
 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中
 成功返回下标，失败返回-1
*/
int32_t pcb_fd_install(int32_t global_fd_idx)
{
    struct task_struct *curr = running_thread();
    uint8_t local_fd_idx = 3;    //跨过stdin, stdout, stderr
    while (local_fd_idx < MAX_FILES_OPEN_PRE_PROC) {
        if (curr->fd_table[local_fd_idx] == -1) {
            curr->fd_table[local_fd_idx] = global_fd_idx;
            break;
        }
        local_fd_idx++;
    }

    if (local_fd_idx == MAX_FILES_OPEN_PRE_PROC) {
        printk("exceed max open files_per_proc.\n");
        return -1;
    }

    return local_fd_idx;
}

/* 分配一个inode,返回inode编号 */
int32_t inode_bitmap_alloc(struct partition *part)
{
    int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
    if (bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->inode_bitmap, bit_idx, 1);
    return bit_idx;
}

/* 分配1个扇区，返回其扇区地址 */
int32_t block_bitmap_alloc(struct partition *part)
{
    int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
    if (bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->block_bitmap, bit_idx, 1);
    /* 和inode_bitmap_malloc不同，此处返回的不是位图索引，而是具体可用的扇区地址 */
    return (part->sb->data_start_lba + bit_idx);
}

/* 将内存中bitmap第bit_idx位所在的512字节同步到硬盘 */
void bitmap_sync(struct partition *part, uint32_t bit_idx, uint8_t btmp)
{
    uint32_t off_sec = bit_idx / BITS_PER_SECTOR;   //本inode索引相对于位图的扇区偏移量
    uint32_t off_size = off_sec * BLOCK_SIZE;       //本inode索引相对于位图的字节偏移量
    uint32_t sec_lba;
    uint8_t *bitmap_off;

    //需要被同步到硬盘的位图只有inode_bitmap和block_bitmap
    switch (btmp) {
        case INODE_BITMAP:
            sec_lba = part->sb->inode_bitmap_lba + off_sec;
            bitmap_off = part->inode_bitmap.bits + off_size;
        break;

        case BLOCK_BITMAP:
            sec_lba = part->sb->block_bitmap_lba + off_sec;
            bitmap_off = part->block_bitmap.bits + off_size;
        break;

        default:
        break;
    }
    ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

/* 创建文件，若成功则返回文件描述符，否则返回-1 */
int32_t file_create(struct dir *parent_dir, char *filename, uint8_t flag)
{
    //后续操作的公共缓冲区
    void *io_buf = sys_malloc(1024);
    if (io_buf == NULL) {
        printk("in file_create: sys_malloc for io_buf failed.\n");
        return -1;
    }

    uint8_t rollback_step = 0;  //用于操作失败时回滚各资源状态

    //为新文件分配inode
    int32_t inode_no = inode_bitmap_alloc(curr_part);
    if (inode_no == -1) {
        printk("in file_create: allocate inode failed.\n");
        return -1;
    }

    /*
     此inode要从堆中申请内存，不可生成局部变量(函数退出时会释放)
     因为file_table数组中的文件描述符的inode指针要指向它
    */
    struct inode *new_file_inode = (struct inode *)sys_malloc(sizeof(struct inode));
    if (new_file_inode == NULL) {
        printk("file_create: sys_malloc for inode failed.\n");
        rollback_step = 1;
        goto rollback;
    }
    inode_init(inode_no, new_file_inode);

    //返回的是file_table数组的下标
    int fd_idx = get_free_slot_in_global();
    if (fd_idx == -1) {
        printk("exceed max open fileds.\n");
        rollback_step = 2;
        goto rollback;
    }

    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = flag;
    file_table[fd_idx].fd_inode->write_deny = false;

    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));
    create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

    //同步内存数据到硬盘
    /*
     a.在目录parent_dir下安装目录项new_dir_entry
     写入硬盘后返回true，否则返回false
    */
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
        printk("sync dir_entry to disk failed.\n");
        rollback_step = 3;
        goto rollback;
    }

    memset(io_buf, 0, 1024);
    /*
     b.将父目录inode的内容同步到硬盘
    */
    inode_sync(curr_part, parent_dir->inode, io_buf);

    memset(io_buf, 0, 1024);
    /*
     c.将新创建文件的inode内容同步到硬盘
    */
    inode_sync(curr_part, new_file_inode, io_buf);

    /*
     d.将inode_bitmap位图同步到硬盘
    */
    bitmap_sync(curr_part, inode_no, INODE_BITMAP);

    /*
     e.将创建的文件inode添加到open_inodes链表
    */
    list_push(&curr_part->open_inodes, &new_file_inode->inode_tag);
    new_file_inode->i_open_cnts = 1;

    sys_free(io_buf);
    return pcb_fd_install(fd_idx);

rollback:
    switch (rollback_step) {
        case 3:
            //失败时，将file_table中的相应位清空
            memset(&file_table[fd_idx], 0, sizeof(struct file));
        case 2:
            sys_free(new_file_inode);
        case 1:
            //如果新文件的inode创建失败，之前位图中分配的inode_no也要恢复
            bitmap_set(&curr_part->inode_bitmap, inode_no, 0);
        break;
    }
    sys_free(io_buf);
    return -1;
}

/*
打开编号为inode_no的inode对应的文件，
若成功则返回文件描述符，否则返回-1
*/
int32_t file_open(uint32_t inode_no, uint8_t flag)
{
    int fd_idx = get_free_slot_in_global();
    if (fd_idx == -1) {
        printk("exceed max open files.\n");
        return -1;
    }
    file_table[fd_idx].fd_inode = inode_open(curr_part, inode_no);
    file_table[fd_idx].fd_pos = 0;  //每次打开文件要将fd_pos还原为0，即让文件内的指针指向开头
    file_table[fd_idx].fd_flag = flag;
    bool *write_deny = &file_table[fd_idx].fd_inode->write_deny;

    if ((flag & O_WRONLY) || (flag & O_RDWR)) {
        //只要是关于写文件，判断是否有其他进程正写此文件；若是读文件，不考虑write_deny
        INTR_STATUS_T old_status = intr_disable();
        if (!(*write_deny)) {
            *write_deny = true;
            intr_set_status(old_status);
        } else {
            intr_set_status(old_status);
            printk("file is busy now. try again later.\n");
            return -1;
        }
    }

    //若是读文件或创建文件，则不关注write_deny
    return pcb_fd_install(fd_idx);
}

int32_t file_close(struct file *file)
{
    if (file == NULL)
        return -1;
    file->fd_inode->write_deny = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;      //使文件结构可用
    return 0;
}

/* 把buf中的count字节写入到文件描述符fd，成功返回写入的字节数，失败返回-1 */
int32_t file_write(struct file *file, const void *buf, uint32_t count)
{
    if ((file->fd_inode->i_size + count) > (140 * BLOCK_SIZE)) {    //目前单个文件最大支持140*512字节
        printk("exceed max file_size 71680 bytes, write file failed.\n");
        return -1;
    }

    uint8_t *io_buf = (uint8_t *)sys_malloc(512);
    if (io_buf == NULL) {
        printk("sys_write: sys_malloc for io_buf failed.\n");
        return -1;
    }

    uint32_t *all_blocks = (uint32_t *)sys_malloc((12 + 128) * 4);
    if (all_blocks == NULL) {
        printk("sys_write: sys_malloc for all_blocks failed.\n");
        goto _write_exit_1;
    }

    const char *src = (char *)buf;
    uint32_t byte_written = 0;      //用来记录已写入数据大小
    uint32_t size_left = count;     //用来记录未写入数据大小
    int32_t block_lba = -1;         //块地址
    uint32_t block_bitmap_idx = 0;  //用来记录block对应block_bitmap中的索引，作为参数传给bitmap_sync

    uint32_t sec_idx;               //用来索引扇区
    uint32_t sec_lba;               //扇区地址
    uint32_t sec_off_bytes;         //扇区内字节偏移量
    uint32_t sec_left_bytes;        //扇区内剩余字节量
    uint32_t chunk_size;            //每次写入硬盘的数据块大小
    int32_t indirect_block_table;   //用来获取一级间接表地址
    uint32_t block_idx;             //块索引

    //判断文件是否是第一次写，如果是则分配一个块
    if (file->fd_inode->i_sectors[0] == 0) {
        block_lba = block_bitmap_alloc(curr_part);
        if (block_lba == -1) {
            printk("file_write: block_bitmap_alloc failed.\n");
            goto _write_exit_2;
        }
        file->fd_inode->i_sectors[0] = block_lba;
        //每分配一块就将位图同步到硬盘
        block_bitmap_idx = block_lba - curr_part->sb->data_start_lba;
        ASSERT(block_bitmap_idx != 0);
        bitmap_sync(curr_part, block_bitmap_idx, BLOCK_BITMAP);
    }

    //获取在写入数据前，该文件已经占用的块数
    uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
    //存储count字节后，文件将占用的块数
    uint32_t file_will_used_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
    ASSERT(file_will_used_blocks <= 140);
    //通过此增量判断是否需要新分配块，如果增量为0，表示原块够用
    uint32_t add_blocks = file_will_used_blocks - file_has_used_blocks;

    if (add_blocks == 0) {
        if (file_will_used_blocks <= 12) {  //数据在直接块内
            block_idx = file_has_used_blocks - 1;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];   //指向最后一个已有数据的扇区
        } else {
            //未写数据之前已经占用了间接块，需要从硬盘中把间接块信息读到内存
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(curr_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    } else {
        //若有增量便涉及到分配新块(1块=1扇区)及是否分配一级间接块表; 下面要分三种情况处理：
        /* 第一种情况：12个直接块够用 */
        if (file_will_used_blocks <= 12) {
            //先将有剩余空间可继续用的扇区地址写入到all_blocks
            block_idx = file_has_used_blocks - 1;
            ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

            //再将未来要使用到的扇区分配好写入到all_blocks中
            block_idx = file_has_used_blocks;   //指向第一个要分配的新扇区
            while (block_idx < file_will_used_blocks) {
                block_lba = block_bitmap_alloc(curr_part);
                if (block_lba == -1) {
                    printk("sys_write: block_bitmap_alloc for situation 1 failed.\n");
                    goto _write_exit_2;
                }
                
                //写文件时不应该存在块未使用，但已经分配扇区的情况，当文件删除时，就会把块地址清0
                ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                //确保尚未分配扇区地址
                file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
                //每分配一个块就将位图同步到硬盘
                block_bitmap_idx = block_lba - curr_part->sb->data_start_lba;
                bitmap_sync(curr_part, block_bitmap_idx, BLOCK_BITMAP);
                block_idx++;
            }
        } else if (file_has_used_blocks <= 12 && file_will_used_blocks > 12) {
        /* 第二种情况：旧数据在12个直接块内，新数据将使用间接块 */
            //先将有剩余空间的可继续用的扇区地址收集到all_blocks
            block_idx = file_has_used_blocks - 1;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];   //指向旧数据所在的最后一个扇区
            //创建一级间接块表
            block_lba = block_bitmap_alloc(curr_part);
            if (block_lba == -1) {
                printk("file_write: block_bitmap_alloc for situation 2 failed.\n");
                goto _write_exit_2;
            }
            ASSERT(file->fd_inode->i_sectors[12] == 0);
            indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;

            block_idx = file_has_used_blocks;
            while (block_idx < file_will_used_blocks) {
                block_lba = block_bitmap_alloc(curr_part);
                if (block_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 2 failed.\n");
                    goto _write_exit_2;
                }

                if (block_idx < 12) {
                    //新创建的0~11块直接存入all_blocks数组
                    ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                    file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
                } else {
                    //间接块只写入到all_blocks数组中，待全部分配完成后一次性同步到硬盘
                    all_blocks[block_idx] = block_lba;
                }
                block_bitmap_idx = block_lba - curr_part->sb->data_start_lba;
                bitmap_sync(curr_part, block_bitmap_idx, BLOCK_BITMAP);
                block_idx++;
            }
            ide_write(curr_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        } else if (file_has_used_blocks > 12) {
            /* 第三种情况：新数据占据间接块 */
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(curr_part->my_disk, indirect_block_table, all_blocks + 12, 1);

            block_idx = file_has_used_blocks;    //第一个未使用的间接块，即已经使用的间接块的下一块
            while (block_idx < file_will_used_blocks) {
                block_lba = block_bitmap_alloc(curr_part);
                if (block_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 3 failed.\n");
                    goto _write_exit_2;
                }
                all_blocks[block_idx++] = block_lba;
                block_bitmap_idx = block_lba - curr_part->sb->data_start_lba;
                bitmap_sync(curr_part, block_bitmap_idx, BLOCK_BITMAP);
            }
            ide_write(curr_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    }

    /* 用到的块地址已经收集到all_blocks中，下面开始写数据 */
    bool first_write_block = true;      //含有剩余空间的块标识
    file->fd_pos = file->fd_inode->i_size - 1;
    while (byte_written < count) {
        memset(io_buf, 0, BLOCK_SIZE);
        sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
        sec_lba = all_blocks[sec_idx];
        sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

        //判断此次写入硬盘的数据大小
        chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
        if (first_write_block) {
            ide_read(curr_part->my_disk, sec_lba, io_buf, 1);
            first_write_block = false;
        }
        memcpy(io_buf + sec_off_bytes, src, chunk_size);
        ide_write(curr_part->my_disk, sec_lba, io_buf, 1);
        src += chunk_size;
        file->fd_inode->i_size += chunk_size;
        file->fd_pos += chunk_size;
        byte_written += chunk_size;
        size_left -= chunk_size;
    }
    inode_sync(curr_part, file->fd_inode, io_buf);

    sys_free(all_blocks);
    sys_free(io_buf);
    return byte_written;

_write_exit_2:
    sys_free(all_blocks);
_write_exit_1:
    sys_free(io_buf);
    return -1;
}

/* 从文件file中读取count个字节写入buf中，成功返回读出的字节数，若到文件尾则返回-1 */
int32_t file_read(struct file *file, void *buf, uint32_t count)
{
    uint8_t *buf_dst = (uint8_t *)buf;
    uint32_t size = count, size_left = count;

    /*
     若要读取的字节数超过了文件可读的剩余量，就用剩余量作为待读取的字节数
    */
    if ((file->fd_pos + count) > file->fd_inode->i_size) {
        size = file->fd_inode->i_size - file->fd_pos;
        size_left = size;
        if (size == 0) {    //若到文件尾，则返回-1
            return -1;
        }
    }

    uint8_t *io_buf = sys_malloc(BLOCK_SIZE);
    if (io_buf == NULL) {
        printk("file_read: sys_malloc for io_buf failed.\n");
        return -1;
    }
    uint32_t *all_blocks = (uint32_t *)sys_malloc(BLOCK_SIZE + 48);
    if (all_blocks == NULL) {
        printk("%s[%d]: sys_malloc for all_blocks failed.\n", __FUNCTION__, __LINE__);
        goto _read_exit_1;
    }

    uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;   //数据所在块的起始地址
    uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;   //数据所在块的终止地址
    uint32_t read_blocks = block_read_start_idx - block_read_end_idx;

    int32_t indirect_block_table;       //用来获取一级间接表地址
    uint32_t block_idx;                 //获取待读的块地址

    if (read_blocks == 0) {     //在同一扇区内读数据，不涉及到跨扇区读取
        ASSERT(block_read_end_idx == block_read_start_idx);
        if (block_read_end_idx < 12) {      //待读的数据在12个直接块之内
            block_idx = block_read_end_idx;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        } else {
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(curr_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    } else {    //若要读多个块
        /* 第一种情况：起始块和终止块属于直接块 */
        if (block_read_end_idx < 12) {      //数据结束所在的块属于直接块
            block_idx = block_read_start_idx;
            while (block_idx <= block_read_end_idx) {
                all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
            }
        } else if (block_read_start_idx < 12 && block_read_end_idx >= 12) {
        /* 第二种情况：待读入的数据跨越直接块和间接块两类 */
            //先将直接块地址写入all_blocks
            block_idx = block_read_start_idx;
            while (block_idx < 12) {
                all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
            }
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            //再将间接块地址写入all_blocks
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(curr_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        } else {
        /* 第三种情况：数据在间接块中 */
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(curr_part->my_disk, indirect_block_table, all_blocks + 12, 1); //将一级间接块表读进来写入到第13个块的位置之后
        }
    }

    //用到的块地址已经收集到all_blocks中，下面开始读数据
    uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
    uint32_t bytes_read = 0;
    while (bytes_read < size) {
        sec_idx = file->fd_pos / BLOCK_SIZE;
        sec_lba = all_blocks[sec_idx];
        sec_off_bytes = file->fd_pos % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
        chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;

        memset(io_buf, 0, BLOCK_SIZE);
        ide_read(curr_part->my_disk, sec_lba, io_buf, 1);
        memcpy(buf_dst, io_buf + sec_off_bytes, chunk_size);

        buf_dst += chunk_size;
        file->fd_pos += chunk_size;
        bytes_read += chunk_size;
        size_left -= chunk_size;
    }

    sys_free(all_blocks);
    sys_free(io_buf);
    return bytes_read;

_read_exit_2:
    sys_free(all_blocks);
_read_exit_1:
    sys_free(io_buf);
    return -1;
}