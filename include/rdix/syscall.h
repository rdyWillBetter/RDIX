#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <common/type.h>

typedef enum syscall_t{
    SYS_NR_TEST,
    SYS_NR_SLEEP,
    SYS_NR_WRITE,
} syscall_t;

u32 test();
void sleep(time_t ms);
int32 write(fd_t fd, char *buf, size_t len);

#endif