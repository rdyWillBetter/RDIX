#ifndef __FS_H__
#define __FS_H__

#include <common/type.h>
#include <common/list.h>
#include <rdix/mutex.h>

#define BLOCK_SIZE 1024
#define SECTOR_SIZE 512

#define SECS_PER_BLOCK (BLOCK_SIZE / SECTOR_SIZE)

#define TASK_FILE_NR 32 // 每个进程可打开的文件数量

typedef enum whence_t
{
    SEEK_SET = 1, // 直接设置偏移
    SEEK_CUR,     // 当前位置偏移
    SEEK_END      // 结束位置偏移
} whence_t;

typedef struct buffer_head{
    char *b_data;   /* 指向缓冲块（块大小为1024 bit） */
    u32 b_blknr; /* 块号 */
    dev_t b_dev;
    u8 b_dirty;     /* 判断缓冲是否与磁盘同步 */
    mutex_t b_lock;    /* 尝试使用自旋锁 */
    u8 b_count;     /* 引用计数 */
    u8 b_vaild;     /* 是否需要更新，bread 通过这位判断是否需要重新从磁盘中读取数据，0代表需要从磁盘更新 */
    ListNode_t *waiter; /* 等待该缓冲块释放的进程 */
    ListNode_t b_hnode; /* hash node */
    ListNode_t b_fnode; /* free node */
} buffer_t;

void buffer_init();
buffer_t *getblk(dev_t dev, u32 block);
buffer_t *bread(dev_t dev, u32 block);
void bwrite(buffer_t *bf);
void brelse(buffer_t *bf);

void buffer_lock(buffer_t *bf);
void buffer_unlock(buffer_t *bf);

void sync_dev(dev_t dev);

#include <fs/minix1.h>
#include <rdix/device.h>

#endif