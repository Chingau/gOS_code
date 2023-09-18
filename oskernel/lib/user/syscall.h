#ifndef __GOS_OSKERNEL_SYSCALL_H
#define __GOS_OSKERNEL_SYSCALL_H
#include "types.h"
#include "dir.h"
#include "fs.h"

enum SYSCALL_NR {
    SYS_GETPID      = 0,
    SYS_WRITE       = 1,
    SYS_MALLOC      = 2,
    SYS_FREE        = 3,
    SYS_FORK        = 4,
    SYS_READ        = 5,
    SYS_CLEAR       = 6,
    SYS_PUTCHAR     = 7,
    SYS_GETCWD,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_LSEEK,
    SYS_UNLINK,
    SYS_MKDIR,
    SYS_OPENDIR,
    SYS_CLOSEDIR,
    SYS_CHDIR,
    SYS_RMDIR,
    SYS_READDIR,
    SYS_REWINDIR,
    SYS_STAT,
    SYS_PS,
    SYS_EXECV,
    SYS_MAX_NR
};

uint32_t getpid(void);
uint32_t write(int32_t fd, const void *buf, uint32_t count);
void *malloc(uint32_t size);
void free(void *ptr);
pid_t fork(void);
ssize_t read(int fd, void *buf, size_t count);
void clear(void);
void putchar(char char_asci);
char *getcwd(char *buf, uint32_t size);
int32_t open(char *pathname, uint8_t flag);
int32_t close(int32_t fd);
int32_t lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t unlink(const char *pathname);
int32_t mkdir(const char *pathname);
struct dir *opendir(const char *name);
int32_t closedir(struct dir *dir);
int32_t rmdir(const char *pathname);
struct dir_entry *readdir(struct dir *dir);
void rewinddir(struct dir *dir);
int32_t stat(const char *path, struct stat *buf);
int32_t chdir(const char *path);
void ps(void);
int32_t execv(const char *path, const char *argv[]);
#endif
