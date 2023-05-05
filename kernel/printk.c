#include <rdix/kernel.h>
#include <common/stdarg.h>
#include <common/console.h>
#include <common/interrupt.h>
#include <common/stdio.h>
#include <rdix/device.h>
#include <common/string.h>
#include <common/assert.h>

static char buf[1024];

void printk(const char *fmt, ...){
    va_list arg;
    va_start(arg, fmt);

    ATOMIC_OPS(vsprintf(buf, fmt, arg););
    va_end(arg);

    /* 屏显函数中已经设置关中断，无需重复设置 */
    device_t *dev = device_find(DEV_CONSOLE, 0);
    assert(dev);
    device_write(dev->dev, buf, 0, 0, 0);
}

/* 将模式串输出到指定字符串 dest */
size_t sprintf(char *dest, const char *fmt, ...){
    va_list arg;
    va_start(arg, fmt);

    vsprintf(dest, fmt, arg);
    va_end(arg);

    return length(dest);
}