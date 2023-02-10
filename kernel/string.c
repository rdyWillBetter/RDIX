#include <common/string.h>
#include <common/type.h>

#include <rdix/kernel.h>

/* 把 src 中的字符串放到 dest 字符串后面 */
char *strcat(char *dest, const char *src){
    char *ptr = dest;

    while (*ptr != '\0') ++ptr;
    while (*src != '\0'){
        *ptr = *src;
        ++ptr;
        ++src;
    }
    *ptr = '\0';

    return ptr;
}

/* 把字符 src 串复制到 dest 中 */
char *strcpy(char *dest, const char *src){
    char *ptr = dest;

    while (*src != '\0'){
        *ptr = *src;
        ++ptr;
        ++src;
    }
    *ptr = '\0';

    return ptr;
}

int length(const char *str){
    int len = 0;
    while (*str++ != '\0'){
        ++len;
    }
    return len;
}

void *memcpy(void *dest, const void *src, size_t n){
    char *ptr0 = (char *)dest;
    const char *ptr1 = (const char*)src;

    while (n--){
        *ptr0++ = *ptr1++;
    }

    return dest;
}

void *memset(void *dest, const char ch, size_t n){
    char *ptr = (char *)dest;

    while (n--){
        *ptr++ = ch;
    }

    return dest;
}