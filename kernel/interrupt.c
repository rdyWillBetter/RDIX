#include <common/interrupt.h>
#include <rdix/kernel.h>
#include <common/io.h>
#include <common/clock.h>
#include <common/time.h>
#include <common/string.h>
#include <common/global.h>
#include <rdix/memory.h>

#define INT_LOG_INFO __LOG("[interrupt]")

#define INT_SIZE 0x40
#define IDT_SIZE 256

/* handle.asm 中设置的中断处理函数数组，需要加载到 idt 中
 * 该种处理函数的主要工作是保护现场，恢复现场，
 * 然后调用 interrupt_func_table[]，进入真正的处理函数 */
extern handler_t handler_table[INT_SIZE];
/* handle.asm 中设置的系统调用处理函数，需要加载到 idt 中
 * 系统调用函数主体部分保存在 syscall_table[] 中，其作用同上 */
extern void syscall_handle(void);

/* 由中断处理函数调用，是中断处理的本质内容 */
handler_t interrupt_func_table[INT_SIZE];

/* 用于载入 idtr */
static idt_pointer p;

/* 中断描述符表 */
static gate_t idt_table[IDT_SIZE];
static char *int_message[] = {
    "#DE error: Devide by zero",
    "#DB RESERVED",
    "-- NMI Interrupt",
    "#BP Breakpoint",
    "#OF error: Overflow",
    "#BR BOUND Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "-- Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack-Segment Fault",
    "#GP General Protection",
    "#PF Page Fault",
    "-- RESERVED",
    "#MF x86 FPU Floating-Point Error",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD Floating-Point Exception",
    "#VE Virtualization Exception",
    "#CP Control Protection Exception",
};

void set_ioredtbl(u8 irq, u8 dest, u32 flag, u8 vector);

u8 irq_override(u8 old_irq);

u32 _ioapic_reg_read(u32 offset);

u32 _ioapic_reg_write(u32 offset, u32 data);

void apic_init();

/* 通过设置掩码禁用 8259a，禁用后才能用 apic */
void disable_pic(){
    port_outb(PIC_M_H, 0b11111111);
    port_outb(PIC_S_H, 0b11111111);
}

static void sys_exception(
    u32 int_num, u32 code,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags){

    char *message = NULL;
    if (int_num < 22)
    {
        message = int_message[int_num];
    }
    else
    {
        message = int_message[15];
    }

    printk("\nEXCEPTION : %s \n", message);
    printk("   VECTOR : 0x%02X\n", int_num);
    printk("    ERROR : 0x%08X\n", error);
    printk("   EFLAGS : 0x%08X\n", eflags);
    printk("       CS : 0x%02X\n", cs);
    printk("      EIP : 0x%08X\n", eip);
    printk("      ESP : 0x%08X\n", esp);
    printk("       DS : 0x%08X\n", ds);
    printk("       ES : 0x%08X\n", es);
    printk("       fS : 0x%08X\n", fs);
    printk("       GS : 0x%08X\n", gs);
    printk("      EAS : 0x%08X\n", eax);
    printk("      ESP : 0x%08X\n", esp);
    // 阻塞
    while(true);
}

/* =============================================================================
 * bug 调试记录
 * 重启和关机后在开机，计算机表现的特性是不一样的。
 * 在操作系统开发过程中，遇到过重启出现一直触发irq7中断的问题，无法通过mask关闭中断通道，
 * 但是关机再开机后表现正常。
 * ============================================================================= */
static void default_exception(u32 int_num, u32 code){
    printk("default exception, in interrupt [0x%x]\n", int_num);
    lapic_send_eoi();
}

static void idt_init(){
    p.base = (u32)idt_table;
    p.limit = sizeof(idt_table) - 1;

    memset((void *)idt_table, 0, sizeof(idt_table));

    for (size_t i = 0; i < INT_SIZE; ++i){
        idt_table[i].offset_l = (u32)handler_table[i];
        idt_table[i].offset_h = (u32)handler_table[i] >> 16;
        idt_table[i].selector = 1 << 3; //代码段选择子
        idt_table[i].reserved = 0;
        idt_table[i].type = 0xe;        //0b1110，中断门
        idt_table[i].segment = 0;
        idt_table[i].DPL = 0;           //内核态
        idt_table[i].present = 1;
    }

    for (size_t i = 0; i < INTEL_INT_RESERVED; ++i){
        interrupt_func_table[i] = sys_exception;
    }

    interrupt_func_table[0xe] = page_fault;

    for (size_t i = INTEL_INT_RESERVED; i < INT_SIZE; ++i){
        interrupt_func_table[i] = default_exception;
    }
    /* ===============================================================================
     * bug 记录
     * 因为将 syscall_handle 声明的时候写成函数指针的形式（void (*syscall_handle)(void)）
     * (u32)syscall_handle 的值就变成了解引用的值 *syscall_handle
     * 不知道为什么这里没有解引用，但是编译器自动解引用了
     * 应该是函数指针的特性问题
     * 待学习
     * =============================================================================== */
    gate_t *syscall_entry = &idt_table[0x80];
    syscall_entry->offset_l = (u32)syscall_handle;
    syscall_entry->offset_h = ((u32)syscall_handle >> 16);

    /* 目标代码段的 DPL 代表了调用者的最高特权级 */
    syscall_entry->selector = KERNEL_CODE_SEG << 3 | DPL_KERNEL;
    syscall_entry->reserved = 0;
    syscall_entry->type = 0b1110;       //0b1110，中断门
    syscall_entry->segment = 0;         //系统段

    /* 中断描述符的特权级代表了调用者的最低特权级 */
    /* 这样就形成了门的形状 */
    /*==============目标代码段选择子的 DPL ============*/
    /*                                               */
    /*                                               */
    /*                                               */
    /*                                               */
    /*                 中间空白部分                   */
    /*                  都是可以                      */
    /*                   通过的                       */
    /*                                               */
    /*                                               */
    /*                                               */
    /*=================中断描述符特权级================*/
    syscall_entry->DPL = DPL_USER;
    syscall_entry->present = 1;

    asm volatile("lidt p");
}

/* dest 为目标 cpu 的编号 */
void install_int(u8 old_irq, u8 dest, u32 flag, handler_t handler){
    u8 new_irq = irq_override(old_irq);
    u8 vector = new_irq + START_INT_NUM;

    interrupt_func_table[new_irq + 0x20] = handler;
    set_ioredtbl(new_irq, dest, flag, vector);
}

void set_int_mask(u32 irq, bool enable){
    u8 new_irq = irq_override(irq);
    u32 offset = new_irq * 2 + 0x10;

    u32 lo = _ioapic_reg_read(new_irq * 2 + 0x10);

    lo = enable ? lo & ~__IOREDTBL_MASK : lo | __IOREDTBL_MASK;

    _ioapic_reg_write(offset, lo);
}

bool get_IF(){

    asm volatile(
        "pushfl\n"        // 将当前 eflags 压入栈中
        "popl %eax\n"     // 将压入的 eflags 弹出到 eax
        "shrl $9, %eax\n" // 将 eax 右移 9 位，得到 IF 位
        "andl $1, %eax\n" // 只需要 IF 位
    );
}

// 设置 IF 位
void set_IF(bool state)
{
    if (state)
        asm volatile("sti\n");
    else
        asm volatile("cli\n");
}

bool get_and_disable_IF(){
    bool state = get_IF();
    set_IF(false);

    return state;
}

void interrupt_init(){
    idt_init(); //初始化 idt 中断表
    apic_init();
    
    clock_init();
    //rtc_init();
    
    /* 工控机上开启键盘中断会导致问题
     * apic 中断下....因为 irq_override 的问题，导致 ioapic 没有正确设置。 */
    keyboard_init();
}