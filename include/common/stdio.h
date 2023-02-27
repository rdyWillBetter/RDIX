#ifndef __STDIO_H__
#define __STDIO_H__

#include <common/type.h>
#include <common/console.h>
#include <common/stdarg.h>

u32 printf(const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list arg);

#endif