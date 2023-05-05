#include <fs/fs.h>
#include <common/bitmap.h>
#include <common/assert.h>
#include <common/interrupt.h>
#include <common/string.h>

/* 从磁盘中分配一个块 */
idx_t balloc(dev_t dev){
    super_block_t *sb = get_super(dev);
    idx_t block_nr = EOF;

    for (size_t i = 0; i < sb->desc->zmap_blocks; ++i){
        buffer_t *buf = sb->zmaps[i];
        bitmap_t map = {.start = buf->b_data, 
                        .length = BLOCK_SIZE, 
                        .offset = BLOCK_BITS * i + sb->desc->firstdatazone - 1};

        ATOMIC_OPS(block_nr = bitmap_scan(&map, 1);)

        if (block_nr != EOF){
            assert(block_nr < sb->desc->zones);
            ATOMIC_OPS(buf->b_dirty = true;)
            break;
        }
    }
    assert(block_nr != EOF);
    return block_nr;
}

void bfree(dev_t dev, idx_t block_nr){
    super_block_t *sb = get_super(dev);
    assert(block_nr < sb->desc->zones);

    idx_t bmap_nr = block_nr / BLOCK_BITS;

    buffer_t *buf = sb->zmaps[bmap_nr];
    bitmap_t map = {.start = buf->b_data, 
                    .length = BLOCK_SIZE, 
                    .offset = BLOCK_BITS * bmap_nr + sb->desc->firstdatazone - 1};

    assert(bitmap_test(&map, block_nr));

    ATOMIC_OPS(
    bitmap_set(&map, block_nr, 0);
    buf->b_dirty = true;)
}

inode_t ialloc(dev_t dev){
    super_block_t *sb = get_super(dev);
    idx_t inode_nr = EOF;

    for (size_t i = 0; i < sb->desc->imap_blocks; ++i){
        buffer_t *buf = sb->imaps[i];
        bitmap_t map = {.start = buf->b_data, 
                        .length = BLOCK_SIZE, 
                        .offset = BLOCK_BITS * i};// inode 编号是从 1 开始的

        ATOMIC_OPS(inode_nr = bitmap_scan(&map, 1);)

        if (inode_nr != EOF){
            assert(inode_nr < sb->desc->inodes);
            ATOMIC_OPS(buf->b_dirty = true;)
            break;
        }
    }

    return inode_nr;
}

void ifree(dev_t dev, idx_t inode_nr){
    super_block_t *sb = get_super(dev);
    assert(inode_nr < sb->desc->inodes);

    idx_t bmap_nr = inode_nr / BLOCK_BITS;

    buffer_t *buf = sb->imaps[bmap_nr];
    bitmap_t map = {.start = buf->b_data, 
                    .length = BLOCK_SIZE, 
                    .offset = BLOCK_BITS * bmap_nr};

    assert(bitmap_test(&map, inode_nr));
    
    ATOMIC_OPS(
    bitmap_set(&map, inode_nr, 0);
    buf->b_dirty = true;)
}

// 获取 inode 第 block 块的索引值
// 如果不存在 且 create 为 true，则创建
idx_t bmap(m_inode *inode, idx_t block, bool create){
    super_block_t *sb = get_super(inode->dev);
    buffer_t *buf_1 = NULL, *buf_2 = NULL, *buf = inode->buf;
    // 确保 block 合法
    assert(block >= 0 && block < sb->desc->zones);

    // 数组索引
    u16 index = block;

    // 数组
    u16 *array = inode->desc->zone;

    // 当前处理级别
    int level = 0;

    // 当前子级别块数量
    int divider = 1;

    // 直接块
    if (block < 7)
    {
        goto reckon;
    }

    block -= 7;

    if (block < 7 + ZONES_IDX_PER_BLOCK)
    {
        index = 7;
        level = 1;
        divider = 1;
        goto reckon;
    }

    block -= 7 + ZONES_IDX_PER_BLOCK;
    assert(block < ZONES_IDX_PER_BLOCK * ZONES_IDX_PER_BLOCK + 7 + ZONES_IDX_PER_BLOCK);
    index = 8;
    level = 2;
    divider = ZONES_IDX_PER_BLOCK;

reckon:
    for (; level >= 0; level--)
    {
        // 如果不存在 且 create 则申请一块文件块
        if (!array[index] && create)
        {
            array[index] = balloc(inode->dev);

            /* 索引块创建后必须清零 */
            buffer_t *tmp = bread(inode->dev, array[index]);

            buffer_lock(tmp);
            memset(tmp->b_data, 0, BLOCK_SIZE);
            tmp->b_dirty = true;
            buffer_unlock(tmp);

            brelse(tmp);

            ATOMIC_OPS(buf->b_dirty = true;)
        }

        // 如果 level == 0 或者 索引不存在，直接返回
        if (level == 0 || !array[index])
        {
            u16 tmp = array[index];

            if (buf_1)
                brelse(buf_1);
            if (buf_2)
                brelse(buf_2);

            return tmp;
        }

        // level 不为 0，处理下一级索引
        index = block / divider;
        block = block % divider;
        divider /= ZONES_IDX_PER_BLOCK;

        if (level == 2){
            buf_2 = bread(inode->dev, array[index]);
            array = (u16 *)buf_2->b_data;
            buf = buf_2;
        }
        if (level == 1){
            buf_1 = bread(inode->dev, array[index]);
            array = (u16 *)buf_1->b_data;
            buf = buf_1;
        }
    }
}