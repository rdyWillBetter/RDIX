#ifndef __MULTIBOOT_H__
#define __MULTIBOOT_H__

#include <common/type.h>

#define MULTIBOOT_OS_MAGIC 0x36d76289
#define MULTIBOOT_INFO_TYPE_END 0
#define MEM_MAP_TYPE 6

/* grub 提供的 boot infomation 固定信息
 * boot infomation 主要包括：固定信息和一系列 tag */
typedef struct multiboot2_fixed_info_t{
    size_t total_size;  //包含固定头部的总大小
    u32 reserved;
} MFI_t;

/* tag 的固定头部 */
typedef struct multiboot2_tag_t{
    u32 type;   //指明是什么类型的 tag
    size_t size;   //该 tag 的大小
} MTAG_t;

typedef struct multiboot2_mem_entry_t{
    u64 base_addr;
    u64 length;
    u32 type;
    u32 reserved;
} MENTRY_t;

/* 内存位图的 tag 格式 */
typedef struct multiboot2_map_tag_t{
    MTAG_t general;
    u32 entry_size; //每项内存信息的大小
    u32 entry_version;
    MENTRY_t entry[0];
} MAP_TAG_t;

#endif