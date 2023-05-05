#include <fs/fs.h>
#include <rdix/kernel.h>
#include <common/assert.h>
#include <common/string.h>

#define SUPER_NR 32

static super_block_t super_table[SUPER_NR];

super_block_t *get_free_super(){
    for (size_t i = 0; i < SUPER_NR; i++)
    {
        super_block_t *sb = &super_table[i];
        if (sb->dev == EOF)
        {
            return sb;
        }
    }
    PANIC("no more super block!!!");
}

super_block_t *read_super(dev_t dev){
    super_block_t *sb = get_free_super();

    sb->dev = dev;
    sb->buf = bread(dev, 1);
    sb->desc = (d_super_block *)sb->buf->b_data;

    assert(sb->desc->magic == MINIX1_MAGIC);

    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));

    // 读取 inode 位图
    int idx = 2; // 位图从第 2 块开始，第 0 块 引导块，第 1 块 超级块

    for (int i = 0; i < sb->desc->imap_blocks; i++)
    {
        assert(i < IMAP_NR);
        sb->imaps[i] = bread(dev, idx);
        idx++;
    }

    for (int i = 0; i < sb->desc->zmap_blocks; i++)
    {
        assert(i < ZMAP_NR);
        sb->zmaps[i] = bread(dev, idx);
        idx++;
    }

    return sb;
}

// 获得设备 dev 的超级块
super_block_t *get_super(dev_t dev)
{
    for (size_t i = 0; i < SUPER_NR; i++)
    {
        super_block_t *sb = &super_table[i];
        if (sb->dev == dev)    
            return sb;
    }
    
    return read_super(dev);
}

void super_init(){
    for (size_t i = 0; i < SUPER_NR; i++)
    {
        super_block_t *sb = &super_table[i];
        sb->dev = EOF;
        sb->desc = NULL;
        sb->buf = NULL;
        sb->iroot = NULL;
        sb->imount = NULL;
    }
}