#include <rdix/kernel.h>
#include <common/stdarg.h>
#include <common/type.h>

static char buf[1024];

void debugk(const char *file, int line, const char* fmt, ...){
    va_list arg;
    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    printk("DEBUG: file [%s], line [%d]: %s", file, line, buf);
}

void panic(const char *file, int line, const char* fmt, ...){
    va_list arg;
    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    printk("PANIC: file [%s], line [%d]: %s", file, line, buf);

    while (true);
}