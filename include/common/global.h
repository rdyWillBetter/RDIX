#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <common/type.h>

#define GDT_SIZE 128

#define BASE_4G 0
#define LIMIT_4G 0xfffff

/* 特权级 */
#define DPL_KERNEL 0
#define DPL_USER 3

/* 粒度位，G=0 代表 limit 的单位是字节
 * G=1 代表 limit 的单位是页(4KB) */
#define GDT_ENTRY_G 0x1

/* 位数选择
 * 对于数据段来说：
 * D/B=1    段上限为 4GB
 *     0    段上限为 64KB
 * 对于代码段来说：
 *     1    采用 32 位寻址方式，并且代码以32位形式解释
 *     0    变为 16 位模式
 * 对于栈段来说：
 *     1    堆栈访问时使用 esp
 *     0    堆栈使用时使用 sp */
#define GDT_ENTRY_D 0x2

/* 64 位扩展，默认为 0 */
#define GDT_ENTRY_L 0x4

/* 有效位 P=1 表明该段描述符有效。最重要的一位 */
#define GDT_ENTRY_P 0x8

/* 是否为系统描述符 S=0 代表系统描述符
 * S=1 代表这个段是代码段或数据段 */
#define GDT_ENTRY_S 0x10

/* 当 S=1 时，type 的含义如下
 * 位11 | 位10 | 位9 | 位8
 *   X  | E/C  | W/R | A 
 * 当 X=0 时表明该段是数据段，此时 位10 和 位9 译为 E 和 W
 * 当 X=1 时表明该段是代码段，此时 位10 和 位9 译为 C 和 R */
#define GDT_ENTRY_TYPE_X 0x20

/* E=0 代表这个数据段向上扩展，=1 表示向下扩展
 * C=1 一致代码，=0 非一致代码（笼统解释，待学习） */
#define GDT_ENTRY_TYPE_EC 0x40

/* W=0 代表这个数据段只读，=1 表示这个数据段可读可写
 * R=1 表示这个代码段可读可执行，=0 代表改代码段不可读 */
#define GDT_ENTRY_TYPE_WR 0x80

/* 该段是否被访问过，A=1 访问过，=0 未访问过 */
#define GDT_ENTRY_TYPE_A 0x100

typedef enum SEG_IDX{
    KERNEL_CODE_SEG = 1,
    KERNEL_DATA_SEG,
    
    /* TSS 描述符中，S = 0，type = 10B1
     * B位表示 busy，B 位为 0 表示任务不繁忙 */
    KERNEL_TSS_SEG,
    USER_CODE_SEG,
    USER_DATA_SEG
} SEG_IDX;

typedef struct descriptor{
    u16 limit_low; //段界限 0~15 位
    u32 base_low : 24; //段基址 0~23 位
    u8 type : 4; //段类型
    u8 is_system : 1; //S位，1 为代码段或数据段，0 为系统段
    u8 DPL : 2; //描述符特权级
    u8 present : 1; //P位，1 为存在
    u8 limit_hight : 4; //段界限 16~19 位
    u8 available : 1; //AVL位，操作系统使用
    u8 long_mode : 1; //L位，1 为64位扩展
    u8 big : 1; //D位，0 为16位代码段，1 为32位
    u8 granularity : 1; //G位，0 为字节粒度，1 为4KB粒度
    u8 base_high; //基地址 24~31 位
} _packed gdt_descriptor;

/*
typedef struct gdt_selector{
    u8 RPL : 2;
    u8 TI : 1; //0 表示从 LDT 加载段寄存器，1 表示从 GDT 加载段寄存器
    u16 index : 13;
} _packed selector;
*/

typedef unsigned short selector;

typedef struct gdt_pointer{
    u16 limit; //limit == sizeof(gdt) - 1
    u32 base;
} _packed gdt_pointer;

typedef struct tss_t
{
    u32 backlink; // 前一个任务的链接，保存了前一个任状态段的段选择子
    u32 esp0;     // ring0 的栈顶地址
    u32 ss0;      // ring0 的栈段选择子
    u32 esp1;     // ring1 的栈顶地址
    u32 ss1;      // ring1 的栈段选择子
    u32 esp2;     // ring2 的栈顶地址
    u32 ss2;      // ring2 的栈段选择子
    u32 cr3;
    u32 eip;
    u32 flags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldtr;          // 局部描述符选择子
    u16 trace : 1;     // 如果置位，任务切换时将引发一个调试异常
    u16 reversed : 15; // 保留不用
    u16 iobase;        // I/O 位图基地址，16 位从 TSS 到 IO 权限位图的偏移
    u32 ssp;           // 任务影子栈指针
} _packed tss_t;

void gdt_init();

#endif