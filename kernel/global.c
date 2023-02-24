#include <common/global.h>
#include <common/string.h>
#include <common/assert.h>
#include <rdix/kernel.h>

tss_t tss;

static gdt_descriptor gdt_table[GDT_SIZE];
static gdt_pointer gpoint;

/* type 包含除 DPL 外的所有内容 */
/* 将索引值为 idx 的位置填上段描述符 */
static selector set_gdt_desc(SEG_IDX idx, size_t base, size_t limit, size_t type, u8 DPL){
    gdt_descriptor *desc = &gdt_table[idx];
    selector sel;

    assert(!(limit & 0xFFF00000 || DPL > 3));

    sel = (idx << 3) | DPL;

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

    return sel;
}

/* 目前 gdt 表只有 4G 代码段和 4G 数据段 */
void gdt_init(){
    selector kernel_code_selector, kernel_data_selector;
    selector tss_selector;
    selector user_code_selector, user_data_selector;

    /* 清空 gdt_table */
    memset((void *)gdt_table, 0, sizeof(gdt_table));

#pragma region /* 加载描述符 */
    /* 加载内核代码段 */
    kernel_code_selector =\
    set_gdt_desc(KERNEL_CODE_SEG, BASE_4G, LIMIT_4G,\
                GDT_ENTRY_G | GDT_ENTRY_D | GDT_ENTRY_P | GDT_ENTRY_S | GDT_ENTRY_TYPE_X,\
                DPL_KERNEL);
    
    /* 加载内核数据段 */
    kernel_data_selector =\
    set_gdt_desc(KERNEL_DATA_SEG, BASE_4G, LIMIT_4G,\
                GDT_ENTRY_G | GDT_ENTRY_D | GDT_ENTRY_P | GDT_ENTRY_S | GDT_ENTRY_TYPE_WR,\
                DPL_KERNEL);

    /* 加载 TSS 段
     * type == 0b1001 */
    tss_selector =\
    set_gdt_desc(KERNEL_TSS_SEG, &tss, sizeof(tss) - 1,\
                GDT_ENTRY_P | GDT_ENTRY_TYPE_X | GDT_ENTRY_TYPE_A,\
                DPL_KERNEL);
    
    /* 加载用户代码段 */
    user_code_selector =\
    set_gdt_desc(USER_CODE_SEG, BASE_4G, LIMIT_4G,\
                GDT_ENTRY_G | GDT_ENTRY_D | GDT_ENTRY_P | GDT_ENTRY_S | GDT_ENTRY_TYPE_X,\
                DPL_USER);
    
    /* 加载用户数据段 */
    user_data_selector =\
    set_gdt_desc(USER_DATA_SEG, BASE_4G, LIMIT_4G,\
                GDT_ENTRY_G | GDT_ENTRY_D | GDT_ENTRY_P | GDT_ENTRY_S | GDT_ENTRY_TYPE_WR,\
                DPL_USER);
#pragma endregion

    /* 使 gpoint 指向 gdt_table */
    gpoint.base = (u32)gdt_table;
    gpoint.limit = (u16)(sizeof(gdt_table) - 1);

    asm volatile("lgdt gpoint");    //将 gpoint 加载到 gdtr 中

    /* 重新加载段寄存器
     * eax 存放代码段选择子，ebx 存放数据段选择子 */
    asm volatile(
        "movw %0, %%ds\n"
        "movw %0, %%ss\n"
        "movw %0, %%es\n"
        "movw %0, %%fs\n"
        "movw %0, %%gs\n"
        "pushl %1\n"
        "pushl $ret\n"
        "ljmp *(%%esp)\n"   /* AT&T 用寄存器指向的内存作为跳转地址需要加 * 号 */
        "ret:\n"
        "popl %1\n"
        "popl %1\n"
        :
        :"a"(kernel_data_selector),"b"(kernel_code_selector)
    );

    /* 清空 tss 以便于初始化 */
    memset((void *)&tss, 0, sizeof(tss));
    /* 0 特权级下栈段选择子 */
    tss.ss0 = kernel_data_selector;
    /* 该任务可使用 io 位图的偏移地址 */
    tss.iobase = sizeof(tss);

    /* 加载 tr 寄存器 */
    asm volatile(
        "ltr %0\n"
        :
        :"a"(tss_selector)
    );

}