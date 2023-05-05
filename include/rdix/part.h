#ifndef __PART_H__
#define __PART_H__

#include <common/type.h>
#include <rdix/device.h>

#define MBR_PART_NR 4 //主分区数
#define PART_NAME_LEN 8

// 分区文件系统
// 参考 https://www.win.tue.nl/~aeb/partitions/partition_types-1.html
typedef enum PART_FS
{
    PART_FS_FAT12 = 1,    // FAT12
    PART_FS_EXTENDED = 5, // 扩展分区
    PART_FS_MINIX = 0x80, // minux
    PART_FS_LINUX = 0x83, // linux
} PART_FS;

/* 保存在磁盘上的分区描述符 */
typedef struct part_entry_t
{
    u8 bootable;             // 引导标志
    u8 start_head;           // 分区起始磁头号
    u8 start_sector : 6;     // 分区起始扇区号
    u16 start_cylinder : 10; // 分区起始柱面号
    u8 system;               // 分区类型字节
    u8 end_head;             // 分区的结束磁头号
    u8 end_sector : 6;       // 分区结束扇区号
    u16 end_cylinder : 10;   // 分区结束柱面号
    u32 start;               // 分区起始物理扇区号 LBA
    u32 count;               // 分区占用的扇区数
} _packed part_entry_t;

/* 保存在磁盘上的启动扇区数据结构 */
typedef struct boot_sector_t
{
    u8 code[446];
    part_entry_t entry[MBR_PART_NR];
    u16 signature;
} _packed boot_sector_t;

/* 保存在内存中的分区数据结构 */
typedef struct part_t
{
    char name[PART_NAME_LEN];  // 分区名称
    device_t *disk;             // 磁盘指针，指向 device_t 类型
    u32 system;              // 分区类型
    u32 start;               // 分区起始物理扇区号 LBA
    u32 count;               // 分区占用的扇区数
} part_t;

void disk_part_install(dev_t disk_idx);

#endif