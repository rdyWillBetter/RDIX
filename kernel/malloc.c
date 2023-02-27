#include <common/type.h>
#include <rdix/memory.h>
#include <rdix/kernel.h>
#include <common/interrupt.h>

/* malloc 和 free 已经做了竞争保护 */

/* 一定要是 16 字节
 * 每一个页被称为一个 bucket，bucket_desc 是一个 bucket 的描述符
 * 一个 bucket 被分成多个固定大小的块，每个块开头四个字节是一个 void 类型指针
 * 这个指针通常指向该 bluck 内下一个空闲块的地址
 * 当这个块被使用时其开头的 void 指针就会被覆盖，不会影响这个块在使用时的大小
 * 当然在这个块被释放时这个指针会被重新填上 */
typedef struct bucket_desc{
    struct bucket_desc *next_desc;  //下一个描述符
    void *page;                     //该描述符所管理的页，需要通过 get_free_page() 分配
    void *freeptr;                  //指向该页中第一个空闲块的指针
    u16 refcnt;                     //已经被申请的块的个数，当 refcnt == 0 时需要释放该描述符管理的页
    u16 reserved;
} _packed bucket_desc;

/* bucket 描述符链表头
 * 文中将整个链表称为：size 大小的内存池 */
typedef struct _bucket_dir{
    size_t size;                    //表明该链表中的所有 bucket 都被分割成一个个 size 大小的块
    bucket_desc *first_bucket;      //指向第一个 bucket
} _bucket_dir;

/* 包含当前未被使用的 bucket 描述符 */
bucket_desc *free_bucket_desc = (bucket_desc *)0;

/* 描述符字典
 * 16 代表这个 bucket_desc 链表中的所有 bluck 都被分割成大小为 16 字节的块用来分配，称为 16字节内存池
 * 因为一个描述符只能管理一个页，所以可分配的最大连续空间就是 4096 字节 */
_bucket_dir bucket_dir[] = {
    {16,    (bucket_desc *)0},
    {32,    (bucket_desc *)0},
    {64,    (bucket_desc *)0},
    {128,   (bucket_desc *)0},
    {256,   (bucket_desc *)0},
    {512,   (bucket_desc *)0},
    {1024,  (bucket_desc *)0},
    {2048,  (bucket_desc *)0},
    {4096,  (bucket_desc *)0},
    {0,     (bucket_desc *)0}
};


/* 当没有空闲 bucket 描述符时调用该初始化buck
 * 分配一个页用来放描述符 */
void bucket_dec_init(){
    bucket_desc *bdesc, *first;
    
    bdesc = first = (bucket_desc *)get_free_page();
    if (!bdesc)
        PANIC("out of memory in bucket_dec_init()");
    
    /* 将这个页中的所有描述符都链接起来 */
    for (int i = PAGE_SIZE / sizeof(bucket_desc); i > 1; --i){
        bdesc->next_desc = bdesc + 1;
        ++bdesc;
    }
    
    /* 使 free_bucket_desc 指向这些描述符 */
    bdesc->next_desc = free_bucket_desc;
    free_bucket_desc = first;
}

void *malloc(size_t requist_size){
    _bucket_dir *bdir;
    bucket_desc *bdesc;
    void *retptr;

    /* 在描述符字典中寻找合适大小的内存池 */
    for (bdir = bucket_dir; bdir->size; ++bdir)
        if (bdir->size >= requist_size)
            break;
    
    /* 当 requist_size 大于 4096 字节时出错 */
    if (!bdir->size){
        printk("malloc called with impossibly large argument (%d)\n",\
                requist_size);
        PANIC("malloc: bad arg");
    }

    /* 在该内存池是否还含有空闲块 */
    for (bdesc = bdir->first_bucket; bdesc; bdesc = bdesc->next_desc){
        if (bdesc->freeptr)
            break;
    }

    /* 关中断，防止竞争的产生 */
    bool IF_stat= get_IF();
    set_IF(false);

    /* 没有找到含有空闲块的 bucket */
    if (!bdesc){
        char *tmp;
        
        /* 系统中也没有可用的描述符 */
        if (!free_bucket_desc)
            bucket_dec_init();

        /* 获取一个描述符 */
        bdesc = free_bucket_desc;
        free_bucket_desc = free_bucket_desc->next_desc;

        /* 将新获取的描述符加入对应内存池链表的头部 */
        bdesc->next_desc = bdir->first_bucket;
        bdir->first_bucket = bdesc;

        /* 给该描述符分配页，并作一些初始化工作 */
        bdesc->freeptr = bdesc->page = tmp = get_free_page();
        bdesc->refcnt = 0;

        /* 分配页失败 */
        if (!bdesc->freeptr){
            PANIC("out of memory in malloc");
        }


        /* 处理刚分配的页，将其划分特定大小的块，同一个内存池中，这个大小是统一的 */
        for (int i = PAGE_SIZE/bdir->size; i > 1; --i){
            /* 在每个块的头部打上标签，也就是指向该 bluck 内下一个块的指针
             * 这个指针会在这个页分配之后被覆盖，不会影响实际使用大小 */
            *((char **)tmp) = tmp + bdir->size;
            tmp += bdir->size;
        }

        /* 最后一个块指向 0 */
        *((char **)tmp) = (char *) NULL;
    }

    /* 找到空闲块后返回空闲块的指针 */
    retptr = bdesc->freeptr;
    /* 描述符则指向下一个空闲块 */
    bdesc->freeptr = *(void **)(bdesc->freeptr);
    /* 被申请的空闲块加一 */
    ++bdesc->refcnt;

    set_IF(IF_stat);

    return retptr;
}


/* size 的作用是加快块的所搜速度，输入正确的 size 时搜索速度为 O(n)
 * kernel.h 中定义了宏 free(obj)，展开后为 free_s(obj, 0)
 * 当 size == 0 时，就相当于全部扫描一遍，时间复杂度为 O(n^2) */
void free_s(void *obj, size_t size){
    _bucket_dir *bdir;
    bucket_desc *bdesc, *prev;  //prev 为前节点指针，用于删除节点操作
    void *obj_page = (void *)((u32)obj & 0xfffff000);   //获取该 obj 所在的页

    bdesc = prev = NULL;

    for (bdir = bucket_dir; bdir->size; ++bdir){
        prev = NULL;

        if (bdir->size < size){
            continue;
        }
        /* 查找管理页 obj_page 的描述符 */
        for (bdesc = bdir->first_bucket; bdesc; bdesc = bdesc->next_desc){
            if (bdesc->page == obj_page)
                goto found_bdesc;
            prev = bdesc;
        }
    }
    PANIC("free_s: can not found such memory");

/* c语言在标签后不允许存在变量定义语句 */
found_bdesc:;
    bool IF_stat = get_IF();
    set_IF(false);
    
    /* 将这个块添加到空闲块链表中，完成回收 */
    *(void **)obj = bdesc->freeptr;
    bdesc->freeptr = obj;
    --bdesc->refcnt;

    /* 这个描述符中的块全都是空闲的，意味着可以释放这个描述符以及该描述符管理的页 */
    if (bdesc->refcnt == 0){
        /* 函数中申明的指针是局部变量，不会受到任务竞争的影响
        * 但是指针所指向的空间都是全局变量
        * 所以完全有可能在执行期间因为任务抢占导致指针所指向的内容发生变化
        * 所以在这里要检查和恢复 prev，prev 指向的内容可能被其他任务释放 */
        if ((prev && prev->next_desc != bdesc) || (!prev && bdir->first_bucket != bdesc)){
            if (bdesc = bdir->first_bucket)
                prev == NULL;
            else
                for (prev = bdir->first_bucket;\
                    prev && prev->next_desc != bdesc;\
                    prev = prev->next_desc);
        }

        /* 描述符位于链表内和表头两种情况要做不同处理 */
        if (prev)
            prev->next_desc = bdesc->next_desc;
        else{
            if (bdir->first_bucket != bdesc)
                PANIC("free_s: malloc bucket chains corrupted");
            bdir->first_bucket = bdesc->next_desc;
        }

        /* 释放该描述符管理的页 */
        free_page(bdesc->page);
        /* 回收描述符 */
        bdesc->next_desc = free_bucket_desc;
        free_bucket_desc = bdesc;
    }

    set_IF(IF_stat);
}