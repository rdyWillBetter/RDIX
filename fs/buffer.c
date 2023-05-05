#include <fs/fs.h>
#include <common/assert.h>
#include <rdix/memory.h>
#include <rdix/task.h>
#include <common/interrupt.h>

#define HASH_CNT 31

static List_t *wait_list;

/* 缓存块释放链表 */
static List_t *free_list;
static List_t *hash_table[HASH_CNT];

/* buffer_t 数据分配头指针 */
static buffer_t *buffer_ptr = (buffer_t *)BUFFER_M_START;
/* buffer_t::b_data 数据分配头指针 */
static void *__data = (void *)BUFFER_M_END;

/* 返回块在哈希表中索引 */
static u32 hash(dev_t dev, u32 block)
{
    return (dev ^ block) % HASH_CNT;
}

static buffer_t *find_from_hash_table(dev_t dev, u32 blknr){
    u32 idx = hash(dev, blknr);
    List_t *list = hash_table[idx];

    for (ListNode_t *node = list->end.next; node != &list->end; node = node->next){
        buffer_t *ptr = (buffer_t *)node->owner;

        if (ptr->b_dev == dev & ptr->b_blknr == blknr)
            return ptr; 
    }

    return NULL;
}

static buffer_t *get_from_hash_table(dev_t dev, u32 blknr){
    buffer_t *bf = NULL;

    /* 这里没有考虑上锁的问题，直接就获取了，以后再改 */
    if (bf = find_from_hash_table(dev, blknr)){
        ++bf->b_count;
    }
        
    return bf;
}

static buffer_t *get_new_buffer(){
    assert(!get_IF()); /* 确保关中断 */
    buffer_t *bf = NULL;

    if ((u32)buffer_ptr + sizeof(buffer_t) < (u32)__data - BLOCK_SIZE){
        bf = buffer_ptr;

        ++buffer_ptr;
        __data -= BLOCK_SIZE;

        bf->b_data = __data;
        bf->b_dev = EOF;
        bf->b_blknr = 0;
        bf->b_count = 0;
        bf->b_dirty = false;
        mutex_init(&bf->b_lock);
        bf->b_vaild = false;
        bf->waiter = NULL;

        /* 这里只需要初始化节点
         * 两个节点的压入应当时同时进行的 */
        node_init(&bf->b_fnode, bf, 0);
        node_init(&bf->b_hnode, bf, 0);
    }

    return bf;
}

static void buffer_remove(buffer_t *bf){
    if (bf->b_fnode.container)
        remove_node(&bf->b_fnode);
    if (bf->b_hnode.container)
        remove_node(&bf->b_hnode);
}

static void buffer_insert(buffer_t *bf){
    /* 从尾部压入，从头部开始找，从而实现LRU策略 */
    list_pushback(free_list, &bf->b_fnode);

    List_t *hlist = hash_table[hash(bf->b_dev, bf->b_blknr)];
    list_push(hlist, &bf->b_hnode);
}

#define _SCORE(bf) (test_lock(&((bf)->b_lock)) + (((bf)->b_dirty) << 1))
#define _BUF(node) ((buffer_t *)(node)->owner)
buffer_t *getblk(dev_t dev, u32 blknr){
    assert(!get_IF()); /* 确保关中断 */
    buffer_t *bf = NULL;

_retry:
    /* 已经储存在内存中的 buffer */
    bf = get_from_hash_table(dev, blknr);
    if (bf)
        return bf;  /* 这里 bf->b_count 要加一 */

    /* 获取新的 buffer */
    bf = get_new_buffer();

    if (bf)
        goto _find_new;

    buffer_t *tmp = NULL;
    /* 从头部开始找，fnode 的压入应当从尾部压入 */
    for (ListNode_t *node = free_list->end.next; node != &free_list->end; node = node->next){
        tmp = _BUF(node);

        if (tmp->b_count)
            continue;
        if (tmp->waiter)
            continue;
        if (!bf)
            bf = tmp;
        if (!_SCORE(bf)) /* 找到的缓冲既没有锁住也没有脏，直接返回 */
            goto _find_new;
        if (_SCORE(tmp) < _SCORE(bf))
            bf = tmp;
    }

    if (!bf){
        block(wait_list, NULL, TASK_BLOCKED);
        goto _retry;
    }

    /* 注意竞争问题 （可能需要重写） */
    if (!bf->b_lock.value){
        assert(!bf->waiter);
        bf->waiter = current_task(); /* 这因该是个原子操作，起到锁的作用 */

        /* block 之后不管这个地方有没有开中断，都会产生竞态问题 */
        block(NULL, NULL, TASK_BLOCKED); // 对应的 unblock 在哪呢 -- buffer_unlock
        bf->waiter = NULL;  /* 释放锁 */

        /* 有可能在阻塞期间，另一个进程直接通过 get_from_hash_table 获取了该缓存块 */
        if (bf->b_count)
            goto _retry;

        /* 应当等锁和脏都为 false 才能继续，后续需要更改 */
    }

    if (bf->b_dirty){
        bwrite(bf); /* 同步 */

        /* 同样的，在调用 bwrite 后，不管中断是否已经被关闭，都会去执行其他任务，因此需要再加一层判断 */
        if (bf->b_count)
            goto _retry;
    }

    /* 有可能另一个进程和该进程申请的缓存块指向同一个块(dev, blknr 相等)，
     * 然后其中一个进程先一步将缓存块放入 hash table，然后本任务去 hash table 里找就可以了 */
    if (find_from_hash_table(dev, blknr))
        goto _retry;

/* 这里找到的 buffer 是要被重新覆盖的 */
_find_new:
    bf->b_count = 1;
    bf->b_dirty = false;
    bf->b_vaild = false;
    bf->b_blknr = blknr;
    bf->b_dev = dev;
    buffer_remove(bf);
    buffer_insert(bf);

    return bf;
}

void buffer_lock(buffer_t *bf){
    mutex_lock(&bf->b_lock);
}

void buffer_unlock(buffer_t *bf){
    mutex_unlock(&bf->b_lock);

    if (bf->waiter)
        unblock(bf->waiter);
}

buffer_t *bread(dev_t dev, u32 blknr){
    buffer_t *bf = NULL;

    ATOMIC_OPS(bf = getblk(dev, blknr););

    assert(bf);

    if (bf->b_vaild)
        return bf;

    /* 这里有可能另一个进程也指向这个缓冲块
     * 本进程由于 device_request 后调度到其他任务时，另一个任务读到的 b_vaild 是 false
     * 然后又进行一次 device_request */
    device_request(bf, SECS_PER_BLOCK, bf->b_blknr * SECS_PER_BLOCK, 0, REQ_READ);

    ATOMIC_OPS(bf->b_vaild = true;
            bf->b_dirty = false;)

    return bf;
}

void bwrite(buffer_t *bf){
    assert(bf);

    if (!bf->b_dirty)
        return;
    device_request(bf, SECS_PER_BLOCK, bf->b_blknr * SECS_PER_BLOCK, 0, REQ_WRITE);

    ATOMIC_OPS(bf->b_dirty = false;)
}

void brelse(buffer_t *bf){
    if (!bf)
        return;

    ATOMIC_OPS(--bf->b_count;);

    assert(bf->b_count >= 0);

    if (bf->b_count)
        return;
    
    if (bf->b_dirty)
        bwrite(bf);
    
    /* 释放找不到空闲缓冲块的任务 */
    if (!list_isempty(wait_list))
        unblock(wait_list->end.next);
}

void sync_dev(dev_t dev){
    for (buffer_t *bf = (buffer_t *)BUFFER_M_START; bf < buffer_ptr; ++bf){
        if (bf->b_dirty)
            bwrite(bf);
    }

}

void buffer_init(){
    wait_list = new_list();
    free_list = new_list();

    for (size_t i = 0; i < HASH_CNT; ++i){
        hash_table[i] = new_list();
    }
}