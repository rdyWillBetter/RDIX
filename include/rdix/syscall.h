#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <common/type.h>

typedef enum syscall_t{
    SYS_NR_TEST,
    SYS_NR_SLEEP,
    SYS_NR_WRITE,
    SYS_NR_BRK,
    SYS_NR_FORK,
} syscall_t;

u32 test();
void sleep(time_t ms);
int32 write(fd_t fd, char *buf, size_t len);
int32 brk(void *vaddr);

#endif