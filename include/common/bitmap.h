#ifndef __BITMAP_H__
#define __BITMAP_H__

#include <common/type.h>

/* 位图数据结构，包含：
 * start：
 * length：占用空间长度
 * offset：所管理的虚拟内存的起始位置 */
typedef struct bitmap_t{
    u8 *start;
    u32 length;
    u32 offset; //所管理的虚拟内存的起始位置的页索引号
} bitmap_t;

/* map：所需要修改的位图执政数据结构
 * buf：位图表所存放的物理地址
 * length：位图表所占用的长度，单位为字节
 * offset：该表所管理的虚拟内存的起始页索引号 */
void bitmap_init(bitmap_t *map, u8 *buf, u32 length, u32 offset);

bool bitmap_test(bitmap_t *map, u32 idx);

/// @brief 将位图中 idx 对应位的值设置为 status
/// @param map 
/// @param idx 需要设置的页索引号
/// @param status 设置该页状态为 status
/// @return 当该页在位图中对应位的状态和 status 一致时设置失败，返回EOF
int bitmap_set(bitmap_t *map, u32 idx, bool status);

/* 在位图中寻找连续的，长度为 count 的位，
 * 找到第一个符合要求块的起始偏移地址，加上 map->offset 后得到页索引号，
 * 并且将该内存页对应的位置位，使用完毕后需要手动释放
 * 返回虚拟页的索引号 */
int bitmap_scan(bitmap_t *map, u32 count);

#endif