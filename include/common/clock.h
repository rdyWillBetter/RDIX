#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <common/type.h>

#define COUNTER_0 0x40 //寄存器长度为 8bit
#define COUNTER_1 0x41 //寄存器长度为 8bit
#define COUNTER_2 0x42 //寄存器长度为 8bit
#define CONTROL_R 0x43 //寄存器长度为 8bit

#define TIME_SLICE 50
#define OSCILLATION 1193182 //8253 的最大频率（到底多少查不到）
#define CLOCK_COUNTER (OSCILLATION / TIME_SLICE)

void clock_init();

#endif