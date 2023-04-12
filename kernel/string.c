#include <common/string.h>
#include <common/type.h>

#include <rdix/kernel.h>

bool strcmp(const char *str0, const char *str1, size_t count){
    for (int i = 0; i < count; ++i){
        if (str0[i] != str1[i])
            return false;
    }
    return true;
}

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

/* 把字符 src 串复制长度 count 到 dest 中
 * count 必须要大于 1， 因为要空出一个位置放 '\0' */ 
char *strncpy(char *dest, const char *src, size_t count){
    char *ptr = dest;

    while (*src != '\0' && --count){
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