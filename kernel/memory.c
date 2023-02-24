#include <rdix/memory.h>
#include <rdix/kernel.h>
#include <rdix/multiboot2.h>
#include <common/string.h>
#include <common/bitmap.h>
#include <common/assert.h>

#define MEM_AVAILABLE 1
#define V_BIT_MAP_ADDR 0x4000 //虚拟内存管理表起始地址
#define PAGE_IDX(addr) (addr >> 12) //通过页地址得到页索引
#define PAGE_ADDR(idx) (idx << 12) //通过页索引得到页地址
#define BIOS_MEM_SIZE 0x100000
#define DIDX(addr) (addr >> 22) //得到 addr 的页目录索引号
#define TIDX(addr) ((addr >> 12) & 0x3ff) //得到 addr 的页目表引号

static size_t mem_base; //用于存放 1M 以外最大内存的基地址
static size_t mem_size;
static size_t total_pages;
static size_t free_pages;

static page_idx_t start_available_p_page_idx; //第一个可用的物理页索引
static u8 *p_bit_map; //物理内存管理，一页占 8 bit，用于记录物理页被引用次数
static size_t p_map_pages; //物理内存管理表所占页数

/* 创建内核 TCB 时需要知道内存管理位图地址，因此这里导出
 * 目前内核虚拟内存管理位图放在 0x4000 的位置 */
bitmap_t v_bit_map; //内核虚拟内存管理，一页占 1 bit，用于记录该虚拟页是否被占用

static u32 kernel_page_dir = 0x1000; //内核页目录
static u32 kernel_page_table[] = {0x2000, 0x3000}; //内核页表数组，里面保存每个页表的起始物理地址

static void *get_page(){
    for (page_idx_t i = start_available_p_page_idx; i < total_pages; ++i){
        if (p_bit_map[i])
            continue;
        p_bit_map[i] = 1;
        if (free_pages == 0)
            PANIC("Out of Memory");
        --free_pages;
        return (void *)PAGE_ADDR(i);
    }
    PANIC("Out of Memory");
}

/* info 为指向 int 0x15 返回的内存检测结果的指针 */
static void memory_init(u32 magic, u32 info){
    /* 初始值为 0 的全局变量和未初始化的全局变量是一样的，都是放在 bss 段。值都是随机的
     * 因此这样的全局变量一定要初始化后才能使用 */
    mem_base = 0;
    mem_size = 0;

    printk("before :mem_base = %#p, mem_size = %p\n", mem_base, mem_size);
    printk("before :total_page = %d, free_pages = %d\n", total_pages, free_pages);

    if (magic == RDIX_MAGIC){
        printk("Meminfo from RDIX loader\n");

        /* 内存检测结果条目数量 */
        u32 count = *(u32*)info;
        /* 指向第一个内存检测条目(entry) */
        mem_adrs *info_ptr = (mem_adrs*)(info + 4);

        for (int i = 0; i < count; ++i){
            /* base 和 length 都是64位数据，这里只读取了低 32 位的数据，
            * 数据有丢失，今后可以进一步修改以提高操作系统性能 */
            /*printk("count = %02d, base = %#p, length = %#p, type = %d\n",\
            i,\
            (u32)info_ptr[i].base,\
            (u32)info_ptr[i].length,\
            (u32)info_ptr[i].type);*/

            /* 从 1M 以外的空间选取了一块相对较大的空间作为保护模式下的内存，
            * 1M 以外空间的内存不一定连续，这里只取相对大的一块，很有可能有其他更大的块被浪废了 */
            if (info_ptr[i].type == MEM_AVAILABLE && info_ptr[i].base == BIOS_MEM_SIZE){
                mem_base = info_ptr[i].base;
                mem_size = info_ptr[i].length;
            }
        }
    }
    else if (magic == MULTIBOOT_OS_MAGIC){
        printk("Meminfo from MULTIBOOT\n");

        /* multiboot 提供的 boot infomation */
        MFI_t *boot_info = (MFI_t *)info;
        
        /*=======================================================
         * bug 调试记录
         * (boot_info + 8) 得到的地址并不是 boot_info 的值加上 8
         * 而是 (boot_info + 8 * sizeof(typeof(boot_info)))
         *=======================================================*/
        /* 跳过 boot infomation 前面的固定选项部分，选择 tag */
        MAP_TAG_t *map_tag = (MAP_TAG_t *)(info + sizeof(MFI_t));

        /* 在整个 boot infomation 中寻找 memory map 类型的 tag */
        while (map_tag->general.type != MEM_MAP_TYPE && map_tag->general.type != 0)
            /* 这里 tag.size 不是 8 字节对齐的，比如可能值是 12，具体参考手册
             * 但是 tag 结构体都是 8 字节对齐的，所以 tag.size 应当是 8 的倍数
             * 所以做了对齐处理 */
            map_tag = (MAP_TAG_t *)((u32)map_tag + ((map_tag->general.size + 7) & ~7));
        
        /* 没找到 */
        if ((u32)map_tag >= (u32)boot_info + boot_info->total_size || map_tag->general.type == 0){
            PANIC("Boot infomation error\n");
        }

        /* 指向 entry 数组。entry 中包含内存信息 */
        MENTRY_t *grub_mem_entry = map_tag->entry;
        /* memory map 中 entry 项的个数 */
        size_t mem_tag_count = (map_tag->general.size - sizeof(MAP_TAG_t)) / sizeof(MENTRY_t);

        for (int i = 0; i < mem_tag_count; ++i){
            if (grub_mem_entry[i].type == MEM_AVAILABLE && grub_mem_entry[i].base_addr == BIOS_MEM_SIZE){
                mem_base = grub_mem_entry[i].base_addr;
                mem_size = grub_mem_entry[i].length;
            }
        }
    }

    printk("mem_base = %#p, mem_size = %p\n", mem_base, mem_size);

    /* 这两个 if 好像没什么必要，但也写上吧 */
    if (mem_base % PAGE_SIZE || mem_size % PAGE_SIZE){
        PANIC("mem_base or mem_size is not a multiple of PAGE_SIZE\n");
    }

    if (mem_base != BIOS_MEM_SIZE){
        PANIC("Memory discontinuity: mem_base != BIOS_MEM_SIZE\n");
    }

    total_pages = PAGE_IDX(mem_size) + PAGE_IDX(BIOS_MEM_SIZE); //总内存仅包含 1M 位置以及 1M 以上的一段可用内存。
    free_pages = PAGE_IDX(mem_size);

    /* 物理内存管理表基地址位于 1MB 位置
     * 内核虚拟内存管理表基地址位于 0x4000 位置 */
    p_bit_map = (u8 *)mem_base;
    p_map_pages = PAGE_IDX(total_pages + PAGE_SIZE - 1);
    start_available_p_page_idx = PAGE_IDX(BIOS_MEM_SIZE) + p_map_pages;

    /* 清空物理内存管理表 */
    memset((void *)p_bit_map, 0, p_map_pages * PAGE_SIZE);
    free_pages -= p_map_pages;

    /* 将低 1M 空间内存和物理内存管理表占用空间写入物理内存管理表 */
    for (size_t i = 0; i < start_available_p_page_idx; ++i){
        p_bit_map[i] = 1;
    }

    /* 设置内核的虚拟内存位图（内存占用情况），每个进程都有自己单独的 4G 内存。
     * 每创建一个进程就需要设置虚拟内存位图。
     * 因为内核前 1M 空间已经被占用（实模式占用），所以 length 要减去 mem_base (也可以使用 BIOS_MEM_SIZE，程序里两者目前是混用的)。
     * length 为内核虚拟内存管理表的长度，目前该表只管理内核的前 8M 虚拟内存，所以该表需要占用 2048 bit，也就是 256 byte
     * sizeof(kernel_page_table) == 页表个数 * 4
     * V_BIT_MAP_ADDR == 0x4000，即放置虚拟内存位图表的位置为 0x4000， 极可能出现溢出的情况，需要注意。
     * 这段初始化代码应当和页表初始化放在一起或单独放。放在这里不妥。 */
    u32 length = PAGE_IDX((0x100000 * sizeof(kernel_page_table)) - mem_base) / 8;
    memset((void *)V_BIT_MAP_ADDR, 0, length);
    bitmap_init(&v_bit_map, (u8 *)V_BIT_MAP_ADDR, length, PAGE_IDX(mem_base));//该处执行后 v_bit_map 指针指向 V_BIT_MAP_ADDR

    /* 在内核虚拟内存空间中申请放置 物理页管理表 的内存
     * 物理内存的分配和虚拟内存的分配是相互独立的
     * 物理页的分配一般是靠缺页中断
     * 虚拟内存的分配则需要手工进行
     * 这里是内存初始化，所以两者都要同时手动进行 */
    alloc_kpage(p_map_pages);
    //printk("v_bit_map.offset = %#p\n", v_bit_map.offset);

    //printk("after get page, total pages = %d, free pages = %d\n", total_pages, free_pages);
}

/* cr3 寄存器用于存放页目录索引 */
_inline u32 get_cr3(){
    asm volatile("movl %cr3, %eax");
}

_inline u32 set_cr3(u32 pde){
    asm volatile("movl %%eax, %%cr3"::"a"(pde));
}

/* cr0 最高位寄存器用于开启和关闭分页模式 */
static _inline void  enable_page_mode(){
    asm volatile("movl %%cr0, %%eax\n\t\
                orl $0x80000000, %%eax\n\t\
                movl %%eax, %%cr0":::"%eax");
}

/* 将页表地址 pg_idx 写入页表表项类型数据结构 pte */
static void entry_init(page_entry_t *pte, page_idx_t pg_idx){
    *(u32 *)pte = 0;
    pte->present = 1;
    pte->write = 1;
    pte->user = 1;
    pte->index = pg_idx;
}

/* 映射了内核的内存，一共映射了 8M，
 * 开启了分页模式 */
static void page_mode_init(){
    page_idx_t idx = 0; //当前处理的页的索引号

    /* 清理内核页目录 */
    page_entry_t *pde = (page_entry_t *)kernel_page_dir;
    memset((void *)pde, 0, PAGE_SIZE);

    /* pde 为页目录表，pte 为页表，
     * 页索引项一共 20 位，其中前 10 位用于在页目录中检索，后 20 位用于在页表中检索
     * 一共映射了 8M 内存 */
    for (size_t pte_num = 0; pte_num < sizeof(kernel_page_table)/sizeof(u32); ++pte_num){
        page_entry_t *pte = (page_entry_t *)kernel_page_table[pte_num];
        memset((void *)pte, 0, PAGE_SIZE);
        entry_init(&pde[pte_num], PAGE_IDX((u32)pte));  //在页目录表中填入页表占用信息

        /* 将页索引按原顺序填入页表中 */
        for (page_idx_t tmp_idx = 0; tmp_idx < 1024; ++tmp_idx, ++idx){
            /* 第 0 页不映射，为造成空指针访问，缺页异常，便于排错 */
            if (idx == 0)
                continue;
            entry_init(&pte[tmp_idx], idx); //在页表中填入页的占用信息
            p_bit_map[idx] = 1; //将已映射到内核虚拟内存的物理内存都做好标记
        }
    }
    /* 将页目录自身写到第 1024 个表项（最后一个），
     * 因此页目录自身地址为 0xFFC0_0000*/
    entry_init(&pde[1023], PAGE_IDX((u32)pde));

    /* 加载页目录表 */
    set_cr3((u32)pde);

    /* 启动分页模式 */
    enable_page_mode();
}

/* 只操作虚拟内存
 * count 单位为页
 * 从虚拟内存中申请一块长度为 count 的连续内存，返回内存起始地址指针 */
void *alloc_kpage(u32 count){
    int start_page_idx = bitmap_scan(&v_bit_map, count);
    if (start_page_idx == EOF){
        return NULL;
    }
    return (void *)PAGE_ADDR(start_page_idx);   
}

/* 只操作虚拟内存
 * count 单位为页
 * 通过修改内核虚拟内存表中的值，将起始页索引号为 vaddr 后连续的 count 个页都释放。 */
void free_kpage(void *vaddr, u32 count){
    for (size_t i = 0; i < count; ++i){
        if (bitmap_set(&v_bit_map, PAGE_IDX((u32)vaddr) + i, false) == EOF)
            PANIC("free_kpage: memory free error");
    }   
}

//void mem_test();

void mem_pg_init(u32 magic, u32 info){
    memory_init(magic, info);
    page_mode_init();
    //mem_test();
}

/* void mem_test(){
   // 将 20 M 0x1400000 内存映射到 64M 0x4000000 的位置
    u32 laddr = 0x4000000, paddr = 0x1400000;
    page_entry_t *pde = PDT_L_ADDR;
    page_entry_t *pte = PTB_L_ADDR(laddr);
    page_entry_t *new_pte_p = (page_entry_t *)get_page();

    BMB;
    entry_init(&pde[DIDX(laddr)], PAGE_IDX((u32)new_pte_p));
    memset((void *)pte, 0, PAGE_SIZE);

    for (page_idx_t idx = 0; idx < 1024; ++idx){
        entry_init(&pte[idx], idx + PAGE_IDX(paddr));
    }
    BMB;

    char *ptr = (char *)laddr;
    ptr[0] = 's';
    BMB;
} */

/*
void mem_test(){

    void *mem1 = alloc_kpage(2);
    void *mem2 = alloc_kpage(1);

    printk("start of memory 1 = %p\n", mem1);
    printk("start of memory 2 = %p\n", mem2);


    free_kpage(mem1, 2);
    free_kpage(mem2, 1);
}*/