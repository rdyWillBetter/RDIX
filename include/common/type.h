#ifndef __TYPE_H__
#define __TYPE_H__

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long long
#define bool _Bool
#define NULL ((void *)0)
#define EOF -1

#define true 1
#define false 0

#define _packed __attribute__((packed))
#define _inline __attribute__((always_inline)) inline
// 用于省略函数的栈帧
#define _ofp __attribute__((optimize("omit-frame-pointer")))

typedef u32 size_t;
typedef void *handler_t;
typedef u32 time_t;

typedef int int32;

typedef int32 fd_t;

typedef u32 pid_t;

typedef enum std_fd_t{
    stdin,
    stdout,
    stderr,
} std_fd_t;

#endif