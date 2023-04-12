#ifndef __RDIX_H__
#define __RDIX_H__

#include <common/stdarg.h>
#include <common/type.h>

/* 内核魔数，用于校验错误 */
#define RDIX_MAGIC 0x01011017

/* 日志颜色模板 */
#define __LOG(fmt) "\033[0;36]" fmt "\033[0]\t"
#define __WARNING(fmt) "\033[0;33]" fmt "\t"
#define __ERROR(fmt) "\033[1;31]" fmt "\t"

void printk(const char *fmt, ...);

void debugk(const char *file, const int line, const char *fmt, ...);
void panic(const char *file, const int line, const char *fmt, ...);

void *malloc(size_t requist_size);
void free_s(void *obj, size_t size);
#define free(obj) free_s(obj, 0)

#define BMB asm volatile("xchgw %bx, %bx") // bochs magic breakpoint
#define DEBUGK(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)
#define PANIC(fmt, args...) panic(__BASE_FILE__, __LINE__, fmt, ##args)

#endif