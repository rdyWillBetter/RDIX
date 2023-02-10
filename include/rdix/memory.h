#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <common/type.h>

#define PAGE_SIZE 0x1000
#define PDT_L_ADDR ((page_entry_t *)0xfffff000) //页目录的线性地址，往往放在 4G 空间最后一页
#define PTB_L_ADDR(laddr) ((page_entry_t *)(0xffc00000 | (laddr >> 10 & 0xfffff000))) //输入线性地址，可以得到该地址的页表
#define get_free_page() alloc_kpage(1)
#define free_page(laddr) free_kpage(laddr, 1)

/* int 0x15 返回的内存检测结果格式 */
typedef struct mem_ards{
    u64 base;
    u64 length;
    u32 type;
} _packed mem_adrs;

typedef u32 page_idx_t; //用于表示页地址

/* 页表中条目格式 */
typedef struct page_entry_t
{
    u8 present : 1;  // 在内存中
    u8 write : 1;    // 0 只读 1 可读可写
    u8 user : 1;     // 1 所有人 0 超级用户 DPL < 3
    u8 pwt : 1;      // page write through 1 直写模式，0 回写模式
    u8 pcd : 1;      // page cache disable 禁止该页缓冲
    u8 accessed : 1; // 被访问过，用于统计使用频率
    u8 dirty : 1;    // 脏页，表示该页缓冲被写过
    u8 pat : 1;      // page attribute table 页大小 4K/4M
    u8 global : 1;   // 全局，所有进程都用到了，该页不刷新缓冲
    u8 ignored : 3;  // 该安排的都安排了，送给操作系统吧
    page_idx_t index : 20;  // 页索引
} _packed page_entry_t;

void mem_pg_init(u32 magic, u32 info);

/* count 单位为页
 * 从该进程的虚拟内存中申请一块长度为 count 的连续内存，返回内存起始地址指针 */
void *alloc_kpage(u32 count);

/* 释放的是虚拟内存，不是物理内存 */
void free_kpage(void *vaddr, u32 count);

#endif