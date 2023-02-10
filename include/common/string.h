#ifndef __STRING_H__
#define __STRING_H__

#include <common/type.h>

int length(const char *str);

/* 把 src 中的字符串放到 dest 字符串后面 */
char *strcat(char *dest,const char *src);

/* 把字符 src 串复制到 dest 中 */
char *strcpy(char *dest, const char *src);

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dest, const char ch, size_t n);

#endif