#include "types.h"
#include "global.h"
#include "mm.h"
#include "fs.h"
#include "thread.h"
#include "string.h"

extern void intr_exit(void);
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

//32位elf头
typedef struct {
    unsigned char	e_ident[16];	    /* Magic number and other info */
    Elf32_Half	e_type;			/* Object file type */
    Elf32_Half	e_machine;		/* Architecture */
    Elf32_Word	e_version;		/* Object file version */
    Elf32_Addr	e_entry;		/* Entry point virtual address */
    Elf32_Off   e_phoff;		/* Program header table file offset */
    Elf32_Off   e_shoff;		/* Section header table file offset */
    Elf32_Word	e_flags;		/* Processor-specific flags */
    Elf32_Half	e_ehsize;		/* ELF header size in bytes */
    Elf32_Half	e_phentsize;	/* Program header table entry size */
    Elf32_Half	e_phnum;		/* Program header table entry count */
    Elf32_Half	e_shentsize;	/* Section header table entry size */
    Elf32_Half	e_shnum;		/* Section header table entry count */
    Elf32_Half	e_shstrndx;		/* Section header string table index */
} Elf32_Ehdr;

//程序头
typedef struct {
    Elf32_Word	p_type;			/* Segment type */
    Elf32_Off	p_offset;		/* Segment file offset */
    Elf32_Addr	p_vaddr;		/* Segment virtual address */
    Elf32_Addr	p_paddr;		/* Segment physical address */
    Elf32_Word	p_filesz;		/* Segment size in file */
    Elf32_Word	p_memsz;		/* Segment size in memory */
    Elf32_Word	p_flags;		/* Segment flags */
    Elf32_Word	p_align;		/* Segment alignment */
} Elf32_Phdr;

//段类型
#define	PT_NULL		0		/* Program header table entry unused */
#define PT_LOAD		1		/* Loadable program segment */
#define PT_DYNAMIC	2		/* Dynamic linking information */
#define PT_INTERP	3		/* Program interpreter */
#define PT_NOTE		4		/* Auxiliary information */
#define PT_SHLIB	5		/* Reserved */
#define PT_PHDR		6		/* Entry for header table itself */

/*
将文件描述符fd指向的文件中，偏移为offset，大小为filesz的段加载到虚拟地址为vaddr的内存处
*/
static bool segment_load(int32_t fd, uint32_t offset, uint32_t filesz, uint32_t vaddr)
{
    uint32_t vaddr_first_page = vaddr & 0xfffff000; //vaddr所在的页框
    uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000fff);
    uint32_t occupy_pages = 0;

    //若一个页框容不下该段
    if (filesz > size_in_first_page) {
        uint32_t left_size = filesz - size_in_first_page;
        occupy_pages = DIV_ROUND_UP(left_size, PG_SIZE) + 1;    //+1是指vaddr_first_page
    } else {
        occupy_pages = 1;
    }

    //为进程分配内存
    uint32_t page_idx = 0;
    uint32_t vaddr_page = vaddr_first_page;
    while (page_idx < occupy_pages) {
        uint32_t *pde = pde_ptr(vaddr_page);
        uint32_t *pte = pte_ptr(vaddr_page);

        /* 如果pde不存在，或者pte不存在就分配内存，
           pde的判断要在pte之前，否则pde若不存在会导致判断pte时缺页异常 */
        if (!(*pde & 0x00000001) || !(*pte & 0x00000001)) {
            if (get_a_page(PF_USER, vaddr_page) == NULL)
                return false;
        }
        //如果原进程的页表已经分配，利用现有的物理页直接覆盖进程体
        vaddr_page += PG_SIZE;
        page_idx++;
    }
    sys_lseek(fd, offset, SEEK_SET);
    sys_read(fd, (void *)vaddr, filesz);
    return true;
}

/*
从文件系统上加载用户程序pathname,
成功则返回程序的入口地址，否则返回-1
*/
static int32_t load(const char *pathname)
{
    int32_t ret = -1;
    Elf32_Ehdr elf_header;
    Elf32_Phdr prog_header;

    memset(&elf_header, 0, sizeof(Elf32_Ehdr));
    int32_t fd = sys_open(pathname, O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    if (sys_read(fd, &elf_header, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
        ret = -1;
        goto done;
    }

    //校验elf头
    if (memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) || \
        elf_header.e_type != 2 || \
        elf_header.e_machine != 3 || \
        elf_header.e_version != 1 || \
        elf_header.e_phnum > 1024 || \
        elf_header.e_phentsize != sizeof(Elf32_Phdr)) {
        ret = -1;
        goto done;
    }

    Elf32_Off prog_header_offset = elf_header.e_phoff;
    Elf32_Half prog_header_size = elf_header.e_phentsize;

    //遍历所有程序头
    uint32_t prog_idx = 0;
    while (prog_idx < elf_header.e_phnum) {
        memset(&prog_header, 0, prog_header_size);
        //将文件的指针定位到程序头
        sys_lseek(fd, prog_header_offset, SEEK_SET);
        //只获取程序头
        if (sys_read(fd, &prog_header, prog_header_size) != prog_header_size) {
            ret = -1;
            goto done;
        }
        //如果是可加载段就调用segment_load加载到内存
        if (prog_header.p_type == PT_LOAD) {
            if (!segment_load(fd, prog_header.p_offset, prog_header.p_filesz, prog_header.p_vaddr)) {
                ret = -1;
                goto done;
            }
        }
        //更新下一个程序头的偏移
        prog_header_offset += elf_header.e_phentsize;
        prog_idx++;
    }
    ret = elf_header.e_entry;   //程序入口地址
done:
    sys_close(fd);
    return ret;
}

/*
用path指向的程序替换当前进程;
path是用户进程名，argv是传给用户进程的参数
*/
int32_t sys_execv(const char *path, const char *argv[])
{
    uint32_t argc = 0;
    while (argv[argc]) {
        argc++;
    }

    int32_t entry_point = load(path);
    if (entry_point == -1) {
        return -1;
    }

    struct task_struct *curr = running_thread();
    memcpy(curr->name, path, TASK_NAME_LEN);
    curr->name[TASK_NAME_LEN - 1] = 0;

    intr_stack_t *intr_0_stack = (intr_stack_t *)((uint32_t)curr + PG_SIZE - sizeof(intr_stack_t));
    //参数传递给用户进程
    intr_0_stack->ebx = (int32_t)argv;
    intr_0_stack->ecx = argc;
    intr_0_stack->eip = (void *)entry_point;
    intr_0_stack->esp = (void *)0xc0000000; //使新用户进程的栈地址为最高用户空间地址

    //exec不同于fork，为使新进程更快被执行，直接从中断返回
    __asm__ volatile("mov %%esp, %0; jmp intr_exit" : : "g"(intr_0_stack) : "memory");
    return 0;
}