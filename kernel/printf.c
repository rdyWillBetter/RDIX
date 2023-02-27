#include <common/type.h>
#include <common/stdarg.h>
#include <common/stdio.h>
#include <rdix/syscall.h>

char buf[1024];
u32 printf(const char* fmt, ...){
    va_list arg;
    va_start(arg, fmt);

    size_t n = 0;
    
    n = vsprintf(buf, fmt, arg);

    va_end(arg);

    write(stdout, buf, n);

    return n;
}