#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <common/type.h>

typedef enum syscall_t{
    SYS_NR_TEST,
    SYS_NR_SLEEP,
    SYS_NR_WRITE,
    SYS_NR_BRK,
    SYS_NR_FORK,
    SYS_NR_GETPID,
    SYS_NR_GETPPID,
    SYS_NR_EXIT,
    SYS_NR_WAITPID,
    SYS_NR_YIELD,
} syscall_t;

u32 test();
void sleep(time_t ms);
int32 write(fd_t fd, char *buf, size_t len);
int32 brk(void *vaddr);
pid_t fork();
pid_t getpid();
pid_t getppid();
pid_t exit(int status);
pid_t waitpid(pid_t pid, int32 *status);

#endif