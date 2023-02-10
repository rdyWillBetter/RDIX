#ifndef __RTIME_H__
#define __RTIME_H__

#include <common/type.h>

typedef struct time_b{
    u8 tm_sec;
    u8 tm_min;
    u8 tm_hour;
    u8 tm_wday;
    u8 tm_mday;
    u8 tm_mon;
    u8 tm_year;
} time_b;

void time_read(time_b *tm);
time_t mktime(time_b *tm);

// RTC 中断
void rtc_init();

#endif