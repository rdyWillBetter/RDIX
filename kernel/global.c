#include <common/global.h>
#include <common/string.h>

static gdt_descriptor gdt_table[GDT_SIZE];
static gdt_pointer gpoint;

/* type 包含除 DPL 外的所有内容 */
gdt_descriptor* create_desc(gdt_descriptor *desc, size_t base, size_t limit, size_t type, u8 DPL){
    
    if (limit & 0xFFF00000 || DPL & (~(u8)3))
        return NULL;

    memset(desc, 0, sizeof(desc));

    desc->limit_low = (u16)limit;
    desc->limit_hight = (u8)(limit >> 16);

    desc->base_low = base;
    desc->base_high = (u8)(base >> 24);

    desc->DPL = DPL;

    if (type & GDT_ENTRY_G)
        desc->granularity = 1;
    if (type & GDT_ENTRY_D)
        desc->big = 1;
    if (type & GDT_ENTRY_L)
        desc->long_mode = 1;
    if (type & GDT_ENTRY_P)
        desc->present = 1;
    if (type & GDT_ENTRY_S)
        desc->is_system = 1;
    if (type & GDT_ENTRY_TYPE_X)
        desc->type |= 1 << 3;
    if (type & GDT_ENTRY_TYPE_EC)
        desc->type |= 1 << 2;
    if (type & GDT_ENTRY_TYPE_WR)
        desc->type |= 1 << 1;
    if (type & GDT_ENTRY_TYPE_A)
        desc->type |= 1;

    return desc;
}

/* 目前 gdt 表只有 4G 代码段和 4G 数据段 */
void gdt_init(){
    selector code_seg_selector, data_seg_selector;

    code_seg_selector.index = 1;
    code_seg_selector.RPL = 0;
    code_seg_selector.TI = 0;

    data_seg_selector.index = 2;
    data_seg_selector.RPL = 0;
    data_seg_selector.TI = 0;

    /* 将 gpoint 指向的向量表复制到 gdt_table 中 */
    memset((void *)gdt_table, 0, sizeof(gdt_table));

    gdt_descriptor *code_seg = &gdt_table[code_seg_selector.index];
    gdt_descriptor *data_seg = &gdt_table[data_seg_selector.index];

    /* 加载代码段 */
    create_desc(code_seg, BASE_4G, LIMIT_4G,\
                GDT_ENTRY_G | GDT_ENTRY_D | GDT_ENTRY_P | GDT_ENTRY_S | GDT_ENTRY_TYPE_X,\
                DPL_KERNEL);
    /* 加载数据段 */
    create_desc(data_seg, BASE_4G, LIMIT_4G,\
                GDT_ENTRY_G | GDT_ENTRY_D | GDT_ENTRY_P | GDT_ENTRY_S | GDT_ENTRY_TYPE_WR,\
                DPL_KERNEL);

    /* 使 gpoint 指向 gdt_table */
    gpoint.base = (u32)gdt_table;
    gpoint.limit = (u16)(sizeof(gdt_table) - 1);

    asm volatile("lgdt gpoint");    //将 gpoint 加载到 gdtr 中

    /* 重新加载段寄存器 */
    asm volatile(
        "movw %0, %%ds\n"
        "movw %0, %%ss\n"
        "movw %0, %%es\n"
        "movw %0, %%fs\n"
        "movw %0, %%gs\n"
        "pushl %1\n"
        "pushl $ret\n"
        "ljmp *(%%esp)\n"
        "ret:\n"
        "popl %1\n"
        "popl %1\n"
        :
        :"a"(data_seg_selector),"b"(code_seg_selector)
    );
}