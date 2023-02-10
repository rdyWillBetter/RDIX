#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <common/type.h>

typedef enum syscall_t{
    SYS_NR_TEST,
} syscall_t;

u32 test();

#endif