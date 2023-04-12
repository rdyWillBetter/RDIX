#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include <common/type.h>

//#define _INTERRUPT_PIC_MOD

#define PIC_M_L 0x20 //8259A 端口号
#define PIC_M_H 0x21
#define PIC_S_L 0xA0
#define PIC_S_H 0xA1

#define START_INT_NUM 0x20 //起始中断号
#define INTEL_INT_RESERVED 0x20 //intel 使用的前32个中断

#define IRQ0_COUNTER 0x0 //计数器
#define IRQ1_KEYBOARD 0x1
#define IRQ2_CASCADE 0x2 //连接从片，当中断号是大于 8 时，一定要切记这个也要打开！！！
#define IRQ3_SERIAL_0 0x3
#define IRQ4_SWEIAL_1 0x4
#define IRQ5_SERIAL_2 0x5
#define IRQ6_FLOPPY 0x6
#define IRQ7_PARALLEL 0x7
#define IRQ8_RTC 0x8
#define IRQ9_RESERVED 0x9
#define IRQ10_RESERVED 0xa
#define IRQ11_RESERVED 0xb
#define IRQ12_MOUSE 0xc
#define IRQ13_COPROC 0xd
#define IRQ14_DISK 0xe
#define IRQ15_RESERVED 0xf

#define IRQ16_HBA 0x10

typedef struct gate_t{
    u16 offset_l; //代码段，段内偏移低16位
    u16 selector; //代码段，段选择子
    u8 reserved; //保留
    u8 type : 4; //类型
    u8 segment : 1; //0 为系统段，门描述符这一位都为0
    u8 DPL : 2; //使用 int 指令访问的最低特权级
    u8 present : 1; //1 为有效
    u16 offset_h; //段内偏移高16位
} _packed gate_t;

typedef struct idt_pointer{
    u16 limit;
    u32 base;
} _packed idt_pointer;

void interrupt_init();
void syscall_init();
void keyboard_init();
void set_int_mask(u32 irq, bool enable);
bool get_IF();
void set_IF(bool state);
bool get_and_disable_IF();

/* apic */
enum  IOREDTBLFlags{
    __IOREDTBL_MASK = (1 << 16),
    __IOREDTBL_TRIGGER_MODE = (1 << 15),
    __IOREDTBL_DES_MODE = (1 << 11),
};

void lapic_send_eoi();
void install_int(u8 old_irq, u8 dest, u32 flag, handler_t handler);

#endif