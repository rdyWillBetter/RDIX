#include <rdix/memory.h>
#include <rdix/kernel.h>
#include <rdix/multiboot2.h>
#include <common/string.h>
#include <common/bitmap.h>
#include <common/assert.h>
#include <rdix/task.h>
#include <common/interrupt.h>

#define MEMORY_LOG_INFO __LOG("[memory info]")

#define MEM_AVAILABLE_TYPE 1
#define V_BIT_MAP_ADDR 0x4000 //虚拟内存管理表起始地址

/* mmio 所使用的内存空间基地址 */
extern vir_addr_t start_io_memory;

/* 系统所使用的物理内存基地址及大小（不包含低 1M 内存） */
size_t mem_base;
size_t mem_size;

static size_t total_pages; //总物理内存
static size_t free_pages;

static page_idx_t start_available_p_page_idx; //第一个可用的物理页索引，生成以后就固定不变
static u8 *p_bit_map; //物理内存管理，一页占 8 bit，用于记录物理页被引用次数
static size_t p_map_pages; //物理内存管理表所占页数

/* 创建内核 TCB 时需要知道内存管理位图地址，因此这里导出
 * 目前内核虚拟内存管理位图放在 0x4000 的位置 */
bitmap_t v_bit_map; //内核虚拟内存管理，一页占 1 bit，用于记录该虚拟页是否被占用

static u32 kernel_page_dir = 0x1000; //内核页目录
static u32 kernel_page_table[] = {0x2000, 0x3000}; //内核页表数组，里面保存每个页表的起始物理地址

/* 已做竞争保护
 * 返回物理页地址，非索引号 */
static phy_addr_t get_p_page(){
    bool state = get_and_disable_IF();

    for (page_idx_t i = start_available_p_page_idx; i < total_pages; ++i){
        if (p_bit_map[i])
            continue;

        p_bit_map[i] = 1;

        if (free_pages == 0)
            PANIC("Out of Memory");

        --free_pages;

        set_IF(state);
        return (void *)PAGE_ADDR(i);
    }
    PANIC("Out of Memory");
}

/* 已做竞争保护
 * idx 为物理页索引号 */
static void free_p_page(page_idx_t idx){
    /* 确保页索引号在可用物理页范围之内。start_available_p_page_idx 和 total_pages 是一个固定值 */
    assert(idx >= start_available_p_page_idx && idx < total_pages);
    /* 确保该物理页确实有被分配 */
    assert(p_bit_map[idx] >= 1);

    bool state = get_and_disable_IF();

    --p_bit_map[idx];

    if (p_bit_map[idx] == 0)
        ++free_pages;
    
    set_IF(state);

    assert(free_pages > 0 && free_pages < total_pages);
}

/* info 为指向 int 0x15 返回的内存检测结果的指针 */
static void memory_init(u32 magic, u32 info){
    /* 初始值为 0 的全局变量和未初始化的全局变量是一样的，都是放在 bss 段。值都是随机的
     * 因此这样的全局变量一定要初始化后才能使用 */
    mem_base = 0;
    mem_size = 0;

    start_io_memory = IO_MEM_START;

    /*
    printk("before :mem_base = %#p, mem_size = %p\n", mem_base, mem_size);
    printk("before :total_page = %d, free_pages = %d\n", total_pages, free_pages);
    */

    if (magic == RDIX_MAGIC){
        printk(MEMORY_LOG_INFO "Meminfo from RDIX loader\n");

        /* 内存检测结果条目数量 */
        u32 count = *(u32*)info;
        /* 指向第一个内存检测条目(entry) */
        mem_adrs *info_ptr = (mem_adrs*)(info + 4);

        
        for (int i = 0; i < count; ++i){
            /* base 和 length 都是64位数据，这里只读取了低 32 位的数据，
            * 数据有丢失，今后可以进一步修改以提高操作系统性能 */
            /*
            printk("count = %02d, base = %#p, length = %#p, type = %d\n",\
            i,\
            (u32)info_ptr[i].base,\
            (u32)info_ptr[i].length,\
            (u32)info_ptr[i].type);
            */
            /* 从 1M 以外的空间选取了一块相对较大的空间作为保护模式下的内存，
            * 1M 以外空间的内存不一定连续，这里只取相对大的一块，很有可能有其他更大的块被浪废了 */
            if (info_ptr[i].type == MEM_AVAILABLE_TYPE && info_ptr[i].base == BIOS_MEM_SIZE){
                mem_base = info_ptr[i].base;
                mem_size = info_ptr[i].length;
            }
        }
    }
    else if (magic == MULTIBOOT_OS_MAGIC){
        printk(MEMORY_LOG_INFO "Meminfo from MULTIBOOT\n");

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
            if (grub_mem_entry[i].type == MEM_AVAILABLE_TYPE && grub_mem_entry[i].base_addr == BIOS_MEM_SIZE){
                mem_base = grub_mem_entry[i].base_addr;
                mem_size = grub_mem_entry[i].length;
            }
        }
    }

    printk(MEMORY_LOG_INFO "accessible mem_base = %#p, mem_size = %p\n", mem_base, mem_size);

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
    u32 length = PAGE_IDX(KERNEL_MEMERY_SIZE - mem_base) / 8;
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
u32 get_cr3(){
    asm volatile("movl %cr3, %eax");
}

u32 set_cr3(u32 pde){
    asm volatile("movl %%eax, %%cr3"::"a"(pde));
}

/* cr2 含有导致页错误的线性地址，缺页中断时需要使用 */
u32 get_cr2(){
    asm volatile("movl %cr2, %eax");
}

u32 set_cr2(u32 pde){
    asm volatile("movl %%eax, %%cr2"::"a"(pde));
}

/* cr0 最高位寄存器用于开启和关闭分页模式 */
static _inline void  enable_page_mode(){
    asm volatile("movl %%cr0, %%eax\n\t\
                orl $0x80000000, %%eax\n\t\
                movl %%eax, %%cr0":::"%eax");
}

/* 将物理页索引 pg_idx 写入页表表项类型数据结构 pte */
void entry_init(page_entry_t *entry, page_idx_t pg_idx){
    *(u32 *)entry = 0;
    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = pg_idx;
}

/* 映射了内核的内存，一共映射了 8M，
 * 开启了分页模式
 * 内核空间一共是 16M，后 8M 空间并没有进行映射，流出来用于映射 IO 空间 */
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

/* 非内核可使用
 * count 表示申请页的数量，单位为页
 * 只申请虚拟内存空间的页，只有第一次访问该页发生缺页中断时才会分配物理页 */
void *_alloc_page(u32 count){
    bitmap_t *vmap = ((TCB_t *)current_task()->owner)->vmap;
    int start_page_idx = bitmap_scan(vmap, count);
    if (start_page_idx == EOF){
        return NULL;
    }
    return (void *)PAGE_ADDR(start_page_idx);
}

void _free_page(void *vaddr, u32 count){
    bitmap_t *vmap = ((TCB_t *)current_task()->owner)->vmap;
    for (size_t i = 0; i < count; ++i){
        if (bitmap_set(vmap, PAGE_IDX((u32)vaddr) + i, false) == EOF)
            PANIC("_free_page: memory free error");
    }   
}

/* alloc_kpage 和 free_kpage 已经做了竞争保护
 * 只操作内核虚拟内存
 * count 单位为页
 * 从虚拟内存中申请一块长度为 count 的连续内存，返回内存起始地址指针 */
void *alloc_kpage(u32 count){
    bool IF_state = get_IF();
    set_IF(false);

    int start_page_idx = bitmap_scan(&v_bit_map, count);

    set_IF(IF_state);

    if (start_page_idx == EOF){
        return NULL;
    }
    return (void *)PAGE_ADDR(start_page_idx);   
}

/* 只操作内核虚拟内存
 * count 单位为页
 * 通过修改内核虚拟内存表中的值，将虚拟地址 vaddr 后连续的 count 个页都释放。 */
void free_kpage(void *vaddr, u32 count){
    bool IF_state = get_IF();
    set_IF(false);

    for (size_t i = 0; i < count; ++i){
        if (bitmap_set(&v_bit_map, PAGE_IDX((u32)vaddr) + i, false) == EOF)
            PANIC("free_kpage: memory free error");
    }

    set_IF(IF_state);   
}

/* 刷新快表 TLB
 * invlpg m 指令中，m 是内存地址，不是立即数，所以要加中括号（括号） */
static void flush_tlb(u32 vaddr){
    asm volatile(
        "invlpg (%0)\n"
        :
        :"r"(vaddr)
        :"memory"   //为什么要使用 memory ? 应该和编译器有关。
    );
}

/* 获取 vaddr 对应的页表起始地址，若页表不存在则创建一个页表
 * 页和页表的线性地址位于内存空间的最后 4M ，而进程 vmap 最多只能管理 128M + 8M
 * 因此这里页表 pte 的获取不需要在进程的 vmap 里声明 */
page_entry_t *get_pte(u32 vaddr, bool exist){
    page_entry_t *pde = PDE_L_ADDR;
    page_entry_t *pte_entry = &pde[DIDX(vaddr)];

    if (!(pte_entry->present)){
        assert(exist == false);
        entry_init(pte_entry, PAGE_IDX((u32)get_p_page()));
    }
        
    page_entry_t *pte = PTE_L_ADDR(vaddr);
    flush_tlb((u32)pte);

    return pte;
}

void link_page(u32 vaddr){
    page_entry_t *pte = get_pte(vaddr, false);
    page_entry_t *entry = &pte[TIDX(vaddr)];

    TCB_t *task = (TCB_t *)current_task()->owner;
    bitmap_t *vmap = task->vmap;

    /* 当前虚拟内存页必须已经被当前任务申请
     * 如果没有申请，则不可能访问到这一页从而触发缺页中断 */
    assert(bitmap_test(vmap, PAGE_IDX(vaddr)));

    /* page fault 的触发就是根据 present 位来的 */
    if (entry->present)
        return;
    
    u32 paddr = (u32)get_p_page();
    entry_init(entry, PAGE_IDX(paddr));

    /* 将刚申请的页表项载入 TLB */
    flush_tlb(vaddr);

    DEBUGK("link:paddr = 0x%p, vaddr = 0x%p\n", paddr, vaddr);
}

void unlink_page(u32 vaddr){
    page_entry_t *pte = get_pte(vaddr, true);
    page_entry_t *entry = &pte[TIDX(vaddr)];

    assert(entry->present);

    entry->present = false;
    page_idx_t pidx = entry->index;

    free_p_page(pidx);

    /* 不刷新快表的话，cpu 会认为 vaddr 对应的物理地址还存在
     * 并且 present == false 的页表项毫无意义，需要将其从 TLB 中刷掉 */
    flush_tlb(vaddr);

    DEBUGK("unlink:paddr = 0x%p, vaddr = 0x%p\n", PAGE_ADDR(pidx), vaddr);
}

phy_addr_t get_phy_addr(vir_addr_t vaddr){
    *(char *)vaddr = '1';
    page_entry_t *pte = get_pte(vaddr, true);
    page_entry_t *entry = &pte[TIDX((u32)vaddr)];

    phy_addr_t tmp = (phy_addr_t)(PAGE_ADDR(entry->index) + ((u32)vaddr & 0xfff));
    return tmp;
}

page_entry_t *copy_pde(){
    TCB_t *kernel = (TCB_t *)current_task()->owner;
    page_entry_t *pde = (page_entry_t *)alloc_kpage(1);
    memcpy(pde, kernel->pde, PAGE_SIZE);

    page_entry_t *entry = &pde[1023];
    entry_init(entry, PAGE_IDX((u32)pde));

    return pde;
}

#define PAGE_LOG_INFO __LOG("page info")
#define PAGE_ERROR_INFO __ERROR("[page error]")

void page_fault(
    u32 int_num, u32 code,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, page_error_code_t error, u32 eip, u32 cs, u32 eflags){

        assert(int_num == 0xe);
        /* 这里在 USER_STACK_BOTTOM 后面应当专门留出一页来引发缺页中断，防止栈溢出 */

        /* 目前无法在这里做到检测栈是否溢出 */
        /*
        if  (!(esp3 <= USER_STACK_TOP && esp3 > USER_STACK_BOTTOM))
            PANIC("stack error: out of memory!\n");
        */  
        u32 vaddr = get_cr2();

        printk(PAGE_LOG_INFO "in page fault : vaddr = 0x%p\n", vaddr);
        /* USER_STACK_BOTTOM 后面一页不映射，用来引发中断，防止栈溢出 */
        assert(!(vaddr <= USER_STACK_BOTTOM && vaddr > (USER_STACK_BOTTOM - PAGE_SIZE)));
        assert(vaddr >= KERNEL_MEMERY_SIZE && vaddr < USER_STACK_TOP);
        
        if (!error.present){
            link_page(vaddr);
            return;
        }

        printk(PAGE_ERROR_INFO "\nEXCEPTION : PAGE FAULT \n");
        printk(PAGE_ERROR_INFO "   VECTOR : 0x%02X\n", int_num);
        printk(PAGE_ERROR_INFO "    ERROR : 0x%08X\n", error);
        printk(PAGE_ERROR_INFO "   EFLAGS : 0x%08X\n", eflags);
        printk(PAGE_ERROR_INFO "       CS : 0x%02X\n", cs);
        printk(PAGE_ERROR_INFO "      EIP : 0x%08X\n", eip);
        printk(PAGE_ERROR_INFO "      ESP : 0x%08X\n", esp);
        printk(PAGE_ERROR_INFO "       DS : 0x%08X\n", ds);
        printk(PAGE_ERROR_INFO "       ES : 0x%08X\n", es);
        printk(PAGE_ERROR_INFO "       fS : 0x%08X\n", fs);
        printk(PAGE_ERROR_INFO "       GS : 0x%08X\n", gs);
        printk(PAGE_ERROR_INFO "      EAS : 0x%08X\n", eax);

        // 阻塞
        while(true);
    }

void mem_pg_init(u32 magic, u32 info){
    memory_init(magic, info);
    page_mode_init();
}