#include <rdix/kernel.h>
#include <common/console.h>
#include <common/stdarg.h>
#include <common/type.h>
#include <common/interrupt.h>

static char buf[1024];

void debugk(const char *file, int line, const char* fmt, ...){
    va_list arg;
    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    console_put_string("[DEBUG]", WORD_TYPE_DEBUG);
    printk("\tfile [%s], line [%d]: %s", file, line, buf);
}

void logk(const char *info, const char* fmt, ...){
    va_list arg;
    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    char buf_tmp[32];

    vsprintf(buf_tmp, "[%s]", (va_list)&info);
    console_put_string(buf_tmp, WORD_TYPE_LOG);
    printk("\t%s", buf);
}

void panic(const char *file, int line, const char* fmt, ...){
    va_list arg;
    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    console_put_string("[PANIC]", WORD_TYPE_PANIC);
    printk("\tfile [%s], line [%d]: %s", file, line, buf);

    set_IF(false);
    asm volatile("hlt");
}