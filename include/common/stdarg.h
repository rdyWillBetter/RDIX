#ifndef __STDARG_H__
#define __STDARG_H__

typedef char *va_list; //参数表头指针

//四字节对齐后TYPE的长度
#define __va_rounded_size(TYPE) \
    (((sizeof(TYPE) + sizeof(int) - 1) / sizeof (int)) * sizeof(int))

//跳过模式串，获得第二个参数开始的参数列表，LASTARG为固定参数列表中最后一个参数
#define va_start(AP, LASTARG) \
    (AP = ((va_list)&(LASTARG) + __va_rounded_size (LASTARG)))

//返回当前参数指针AP指向的参数的数值，并将AP++
#define va_arg(AP, TYPE) \
    (AP += __va_rounded_size(TYPE), \
    *((TYPE *)(AP - __va_rounded_size(TYPE))))

#define va_end(AP) \
    (AP = (va_list)0)

#endif