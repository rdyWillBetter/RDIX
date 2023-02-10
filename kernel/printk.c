#include <rdix/kernel.h>
#include <common/stdarg.h>
#include <common/console.h>
#include <common/interrupt.h>

char buf[1024];

void printk(const char *fmt, ...){
    va_list arg;
    va_start(arg, fmt);

    bool IF_state = get_IF();
    set_IF(false);

    vsprintf(buf, fmt, arg);
    va_end(arg);

    set_IF(IF_state);

    /* 屏显函数中已经设置关中断，无需重复设置 */
    console_put_string(buf, WORD_TYPE_DEFAULT);
}