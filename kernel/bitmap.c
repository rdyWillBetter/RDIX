#include <common/bitmap.h>
#include <common/string.h>
#include <common/assert.h>
#include <common/interrupt.h>

/* map：所需要修改的位图执政数据结构
 * buf：位图表所存放的物理地址
 * length：位图表所占用的长度，单位为字节
 * offset：该表所管理的虚拟内存的起始页索引号 */
void bitmap_init(bitmap_t *map, u8 *buf, u32 length, u32 offset){
    memset((void *)buf, 0, length);
    map->start = buf;
    map->length = length;
    map->offset = offset;
}

bool bitmap_test(bitmap_t *map, u32 idx){
    assert(idx >= map->offset);

    idx -= map->offset;

    return map->start[idx / 8] & (1 << (idx % 8)) ? true : false;
}

/// @brief 将位图中 idx 对应位的值设置为 status
/// @param map 
/// @param idx 需要设置的页索引号
/// @param status 设置该页状态为 status
/// @return 当该页在位图中对应位的状态和 status 一致时设置失败，返回EOF
int bitmap_set(bitmap_t *map, u32 idx, bool status){

    assert(idx >= map->offset);

    if (bitmap_test(map, idx) == status)
        return EOF;
    
    idx -= map->offset;

    /* 只有 bitmap_set 需要关中断，防止任务竞争 */
    bool IF_stat = get_IF();
    set_IF(false);

    if (status)
        map->start[idx / 8] |= (1 << (idx % 8));
    else
        map->start[idx / 8] &= (~(1 << (idx % 8)));

    set_IF(IF_stat);
    return 0;
}

/* 在位图中寻找连续的，长度为 count 的位，
 * 找到第一个符合要求块的起始偏移地址，加上 map->offset 后得到页索引号，
 * 返回虚拟页的索引号 */
int bitmap_scan(bitmap_t *map, u32 count){
    u32 start_idx = 0;
    u32 local_count = 0;

    while (local_count + start_idx < map->length){
        if (bitmap_test(map, start_idx + local_count + map->offset)){
            start_idx += (local_count + 1);
            local_count = 0;
            continue;
        }
        
        if (local_count == count - 1){
            for (int i = 0; i < count; ++i){
                bitmap_set(map, map->offset + start_idx + i, true);
            }
            return start_idx + map->offset;
        }
        ++local_count;
    }

    return EOF;
}