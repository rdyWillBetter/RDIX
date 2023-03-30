#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <common/type.h>
#include <common/bitmap.h>

#define KERNEL_MEMERY_SIZE 0x1000000
#define IO_MEM_START 0x800000
#define BIOS_MEM_SIZE 0x100000

#define PAGE_SIZE 0x1000
#define PDE_L_ADDR ((page_entry_t *)0xfffff000) //页目录的线性地址，往往放在 4G 空间最后一页
#define PAGE_IDX(addr) (addr >> 12) //通过页地址得到页索引
#define PAGE_ADDR(idx) (idx << 12) //通过页索引得到页地址
#define DIDX(addr) (addr >> 22) //得到 addr 的页目录索引号
#define TIDX(addr) ((addr >> 12) & 0x3ff) //得到 addr 的页表索引号

/* 输入线性地址，返回该地址对应页表的起始地址 */
#define PTE_L_ADDR(vaddr) ((page_entry_t *)(0xffc00000 | (vaddr >> 10 & 0xfffff000)))
#define get_free_page() alloc_kpage(1)
#define free_page(vaddr) free_kpage(vaddr, 1)

/* 用户内存大小是 128M + 内核内存 8M
 * 就是 0x8800000 */
#define USER_STACK_TOP (0x8000000 + KERNEL_MEMERY_SIZE)
#define USER_STACK_SIZE 0x200000
#define USER_STACK_BOTTOM (USER_STACK_TOP - USER_STACK_SIZE)

/* int 0x15 返回的内存检测结果格式 */
typedef struct mem_ards{
    u64 base;
    u64 length;
    u32 type;
} _packed mem_adrs;

typedef u32 page_idx_t; //用于表示页地址

typedef void* phy_addr_t;
typedef void* vir_addr_t;

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

/* 缺页中断时传入的错误码 */
typedef struct page_error_code_t
{
    /* 0 代表是因为页不存在引起的异常
     * 1 代表是由特权级引起的异常 */
    u8 present : 1;

    /* 0 由读引起的异常
     * 1 由写引起的异常 */
    u8 write : 1;

    /* 0 内核态下引起的异常
     * 1 用户态下引起的异常 */
    u8 user : 1;
    u8 reserved0 : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u16 reserved1 : 8;
    u8 sgx : 1;
    u16 reserved2;
} _packed page_error_code_t;

void mem_pg_init(u32 magic, u32 info);

/* count 单位为页
 * 从该进程的虚拟内存中申请一块长度为 count 的连续内存，返回内存起始地址指针 */
void *alloc_kpage(u32 count);

/* 释放的是虚拟内存，不是物理内存 */
void free_kpage(void *vaddr, u32 count);

/* 这两个对应的是非内核版本 */
void *_alloc_page(u32 count);
void _free_page(void *vaddr, u32 count);

/* cr3 寄存器存放页目录物理地址 */
u32 get_cr3();
u32 set_cr3(u32 pde);

void link_page(u32 vaddr);
void unlink_page(u32 vaddr);

page_entry_t *copy_pde();
page_entry_t *get_pte(u32 vaddr, bool exist);
void entry_init(page_entry_t *entry, page_idx_t pg_idx);

phy_addr_t get_phy_addr(vir_addr_t vaddr);

vir_addr_t link_nppage(phy_addr_t addr, size_t size);

void page_fault(
    u32 int_num, u32 code,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, page_error_code_t error, u32 eip, u32 cs, u32 eflags);

#endif