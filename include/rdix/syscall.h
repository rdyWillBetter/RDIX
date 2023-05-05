#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <common/type.h>
#include <fs/fs.h>

typedef enum syscall_t{
    SYS_NR_TEST,
    SYS_NR_SLEEP,
    SYS_NR_BRK,
    SYS_NR_FORK,
    SYS_NR_GETPID,
    SYS_NR_GETPPID,
    SYS_NR_EXIT,
    SYS_NR_WAITPID,
    SYS_NR_YIELD,
    SYS_NR_OPEN,
    SYS_NR_CREATE,
    SYS_NR_CLOSE,
    SYS_NR_READ,
    SYS_NR_WRITE,
    SYS_NR_SEEK,
    SYS_NR_READDIR,
} syscall_t;

u32 test();
void sleep(time_t ms);
int32 brk(void *vaddr);
pid_t fork();
pid_t getpid();
pid_t getppid();
pid_t exit(int status);
pid_t waitpid(pid_t pid, int32 *status);
fd_t open(char *filename, int flags, int mode);
fd_t create(char *filename, int mode);
void close(fd_t fd);
int read(fd_t fd, char *buf, int count);
int write(fd_t fd, char *buf, int count);
int lseek(fd_t fd, idx_t offset, whence_t whence);
int readdir(fd_t fd, dir_entry *dir, u32 count);

#endif