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

    printk("\033[0;32;40]" "[DEBUG]" "\033[0]"\
            "\tfile [%s], line [%d]: %s", file, line, buf);
}

void panic(const char *file, int line, const char* fmt, ...){
    va_list arg;
    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    printk("\033[0]\033[1;31]" "[PANIC]" "\033[0]"\
            "\tfile [%s], line [%d]: %s", file, line, buf);

    set_IF(false);
    asm volatile("hlt");
}