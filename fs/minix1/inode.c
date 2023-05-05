#include <fs/fs.h>
#include <rdix/kernel.h>
#include <common/interrupt.h>
#include <common/string.h>

#define INODE_NR 64
#define BLOCK_INODES (BLOCK_SIZE / sizeof(d_inode))
#define asd sizeof(d_inode)

static m_inode inode_table[INODE_NR];

// 申请一个 inode
static m_inode *get_free_inode()
{
    for (size_t i = 0; i < INODE_NR; i++)
    {
        m_inode *inode = &inode_table[i];
        if (inode->dev == EOF)
        {
            return inode;
        }
    }
    PANIC("no more inode!!!");
}

// 从已有 inode 中查找编号为 nr 的 inode
static m_inode *find_inode(dev_t dev, inode_t nr)
{
    for (int i = 0; i < INODE_NR; ++i){
        if (inode_table[i].dev == dev && inode_table[i].nr == nr)
            return &inode_table[i];
    }
    return NULL;
}

m_inode *get_root(){
    return inode_table;
}

// 计算 inode nr 对应的块号
static inline u32 inode_block(super_block_t *sb, inode_t nr)
{
    // inode 编号 从 1 开始
    return 2 + sb->desc->imap_blocks + sb->desc->zmap_blocks + (nr - 1) / BLOCK_INODES;
}


// 获得设备 dev 的 nr inode
m_inode *iget(dev_t dev, inode_t nr)
{
    assert(nr > 0);
    m_inode *inode = find_inode(dev, nr);
    /* inode 有可能被锁住 */
    if (inode)
    {
        ATOMIC_OPS(++inode->count;);
        //inode->atime = time();

        return inode;
    }

    super_block_t *sb = get_super(dev);

    assert(nr <= sb->desc->inodes);

    ATOMIC_OPS(inode = get_free_inode();)
    inode->dev = dev;
    inode->nr = nr;
    inode->count = 1;

    u32 block = inode_block(sb, inode->nr);
    buffer_t *buf = bread(inode->dev, block);

    inode->buf = buf;

    // 将缓冲视为一个 inode 描述符数组，获取对应的指针；
    inode->desc = &((d_inode *)buf->b_data)[(inode->nr - 1) % BLOCK_INODES];

    inode->ctime = inode->desc->mtime;
    //inode->atime = time();

    return inode;
}

void iput(m_inode *inode){
    assert(inode->count > 0);

    if (inode->count > 1){
        ATOMIC_OPS(--inode->count;);
        return;
    }
    
    brelse(inode->buf);

    ATOMIC_OPS(
        inode->count = 0;
        inode->dev = EOF;
    );
}

// 从 inode 的 offset 处，读 len 个字节到 buf
int inode_read(m_inode *inode, char *buf, u32 len, idx_t offset){
    assert(ISFILE(inode->desc->mode) || ISDIR(inode->desc->mode));

    if (offset >= inode->desc->size)
        return EOF;
    
    size_t total_size = 0;
    u32 ptr = offset;
    while(len && (ptr + total_size) < inode->desc->size){
        idx_t block = bmap(inode, offset / BLOCK_SIZE, false);
        assert(block);  // 可以在 buf 中填充 0
        buffer_t *bf = bread(inode->dev, block);

        size_t size = 0;
        if (inode->desc->size - (ptr + total_size) < len)
            size = inode->desc->size - (ptr + total_size);
        else if (BLOCK_SIZE - offset % BLOCK_SIZE < len)
            size = BLOCK_SIZE - offset % BLOCK_SIZE;
        else
            size = len;

        memcpy(buf, bf->b_data + offset % BLOCK_SIZE, size);

        brelse(bf);
        len -= size;
        buf += size;
        total_size += size;
        offset = (offset / BLOCK_SIZE + 1) * BLOCK_SIZE;
    }

    // 时间

    return total_size;
}

// 从 inode 的 offset 处，将 buf 的 len 个字节写入磁盘
int inode_write(m_inode *inode, char *buf, u32 len, idx_t offset){
    assert(!ISDIR(inode->desc->mode));

    if (len + offset > inode->desc->size)
        inode->desc->size = len + offset;
    while (len){
        idx_t block = bmap(inode, offset / BLOCK_SIZE, true);

        buffer_t *bf = bread(inode->dev, block);

        size_t size = 0;
        if (BLOCK_SIZE - offset % BLOCK_SIZE < len)
            size = BLOCK_SIZE - offset % BLOCK_SIZE;
        else
            size = len;

        buffer_lock(bf);
        memcpy(bf->b_data + offset % BLOCK_SIZE, buf, size);
        bf->b_dirty = true;
        buffer_unlock(bf);

        brelse(bf);
        len -= size;
        buf += size;
        offset = (offset / BLOCK_SIZE + 1) * BLOCK_SIZE;
    }

    return len;
}

static void inode_bfree(m_inode *inode, u16 *array, int index, int level)
{
    if (!array[index])
    {
        return;
    }

    if (!level)
    {
        bfree(inode->dev, array[index]);
        return;
    }

    buffer_t *buf = bread(inode->dev, array[index]);
    for (size_t i = 0; i < ZONES_IDX_PER_BLOCK; i++)
    {
        inode_bfree(inode, (u16 *)buf->b_data, i, level - 1);
    }
    brelse(buf);
    bfree(inode->dev, array[index]);
}

// 释放 inode 所有文件块
void inode_truncate(m_inode *inode)
{
    if (!ISFILE(inode->desc->mode) && !ISDIR(inode->desc->mode))
    {
        return;
    }

    // 释放直接块
    for (size_t i = 0; i < 7; i++)
    {
        inode_bfree(inode, inode->desc->zone, i, 0);
        inode->desc->zone[i] = 0;
    }

    // 释放一级间接块
    inode_bfree(inode, inode->desc->zone, 7, 1);
    inode->desc->zone[7] = 0;

    // 释放二级间接块
    inode_bfree(inode, inode->desc->zone, 8, 2);
    inode->desc->zone[8] = 0;

    inode->desc->size = 0;
    inode->buf->b_dirty = true;
}

void inode_init()
{
    for (size_t i = 0; i < INODE_NR; i++)
    {
        m_inode *inode = &inode_table[i];
        inode->dev = EOF;
    }
}