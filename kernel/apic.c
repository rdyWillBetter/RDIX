#include <rdix/hardware.h>
#include <rdix/memory.h>
#include <common/assert.h>
#include <rdix/kernel.h>
#include <rdix/memory.h>
#include <common/io.h>
#include <common/interrupt.h>

#define APIC_LOG_INFO __LOG("[apic]")

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

/* local apic 寄存器 */
#define LOCAL_APIC_ID_REG 0x20
#define LOCAL_APIC_TPR_REG 0x80
#define LOCAL_APIC_EOI_REG 0xb0
#define LOCAL_APIC_SPURIOUS_REG 0xf0

#define LOCAL_APIC_LVT_TIMER_REG 0x320
#define LOCAL_APIC_LVT_CMCI_REG 0x2f0
#define LOCAL_APIC_LVT_LINT0_REG 0x350
#define LOCAL_APIC_LVT_LINT1_REG 0x360
#define LOCAL_APIC_LVT_ERROR_REG 0x370
#define LOCAL_APIC_LVT_PMC_REG 0x340
#define LOCAL_APIC_LVT_THERMAL_REG 0x330

/* 在伪中断寄存器中启用 apic */
#define IA32_APIC_SOFTWARE_ENABLE 0x100
#define IA32_APIC_LVT_MASK 0x10000

extern volatile u8 *lapic_base;
extern volatile u32 *ioapic_addr;
extern volatile u32 *ioapic_data;

extern struct {
    size_t size;
    u8 tab[16][2];
} irq_override_tab;

void disable_pic();

/* 从 MSR 寄存器中获取 lapic 寄存器的基地址 */
u32 cpu_get_lapic_base(){
   u32 eax, edx;
   cpuGetMSR(IA32_APIC_BASE_MSR, &eax, &edx);
 
   return (eax & 0xfffff000);
}

/* MSR 中硬启动 lapic */
void cpu_enable_apic(){
    u32 eax, edx;
    cpuGetMSR(IA32_APIC_BASE_MSR, &eax, &edx);
    cpuSetMSR(IA32_APIC_BASE_MSR, eax | IA32_APIC_BASE_MSR_ENABLE, edx);
}

void lapic_init(){
    /* 在 MSR 中硬启用 lapic */
    cpu_enable_apic();

    u32 lo, hi;

    cpuGetMSR(IA32_APIC_BASE_MSR, &lo, &hi);

    /* 将 lapic 寄存器物理地址映射到虚拟内存空间 */
    lapic_base = (u8 *)link_nppage(lapic_base, 0x400);

    u32 *sivr_reg = (u32 *)(lapic_base + LOCAL_APIC_SPURIOUS_REG);
    u32 *trp_reg = (u32 *)(lapic_base + LOCAL_APIC_TPR_REG);

    printk(APIC_LOG_INFO "phy addr %p\n", get_phy_addr(sivr_reg));
    printk(APIC_LOG_INFO "MSR %x\n", lo);
    printk(APIC_LOG_INFO "Spurious Interrupt Vector Register %x\n", *sivr_reg);
    printk(APIC_LOG_INFO "lapic id %x\n", *(u32 *)(lapic_base + 0x20) >> 24);
    printk(APIC_LOG_INFO "TPR %x\n", *(u32 *)(lapic_base + 0x80));

    *(u32 *)(lapic_base + LOCAL_APIC_LVT_TIMER_REG) |= IA32_APIC_LVT_MASK;
    *(u32 *)(lapic_base + LOCAL_APIC_LVT_CMCI_REG) |= IA32_APIC_LVT_MASK;
    *(u32 *)(lapic_base + LOCAL_APIC_LVT_LINT0_REG) |= 251;
    *(u32 *)(lapic_base + LOCAL_APIC_LVT_LINT1_REG) |= IA32_APIC_LVT_MASK;
    *(u32 *)(lapic_base + LOCAL_APIC_LVT_ERROR_REG) |= IA32_APIC_LVT_MASK;
    *(u32 *)(lapic_base + LOCAL_APIC_LVT_PMC_REG) |= IA32_APIC_LVT_MASK;
    *(u32 *)(lapic_base + LOCAL_APIC_LVT_THERMAL_REG) |= IA32_APIC_LVT_MASK;

    /* 设置中断优先级为 0，放行所有中断 */
    *trp_reg &= 2 << 4;

    /* 在伪中断寄存器中软启用 lapic */
    *sivr_reg |= IA32_APIC_SOFTWARE_ENABLE;

    //lapic_send_eoi();
}

/* 从 pic 模式转换到 irq 模式后的中断源覆盖 */
u8 irq_override(u8 old_irq){
    for (size_t i = 0; i < irq_override_tab.size; ++i){
        if (old_irq == irq_override_tab.tab[i][0]){
            old_irq = irq_override_tab.tab[i][1];
            return;
        }
    }
}

u32 _ioapic_reg_read(u32 offset){
    *ioapic_addr = offset;
    iodelay();
    return *ioapic_data;
}

u32 _ioapic_reg_write(u32 offset, u32 data){
    *ioapic_addr = offset;
    iodelay();
    *ioapic_data = data;
}

void set_ioredtbl(u8 irq, u8 dest, u32 flag, u8 vector){
    u8 new_irq = irq_override(irq);
    u32 offset = irq * 2 + 0x10;
    flag &= 0x1ff00;

    _ioapic_reg_write(offset, (u32)(vector | flag));
    _ioapic_reg_write(offset + 1, (u32)(dest << 24));
}

void ioapic_init(){
    /* 将两个寄存器地址映射到虚拟内存空间
     * 两个寄存器会直接占用一页，可以尝试重定位两个寄存起的地址 */
    ioapic_addr = (u8 *)link_nppage(ioapic_addr, 0x20);
    ioapic_data = (u32 *)((u32)ioapic_addr + 0x10);

    printk(APIC_LOG_INFO "ioapic id %d\n", _ioapic_reg_read(0));
    printk(APIC_LOG_INFO "ioapic version %d\n", _ioapic_reg_read(1));
    printk(APIC_LOG_INFO "phy ioapic_addr %p\n", get_phy_addr(ioapic_addr));
    printk(APIC_LOG_INFO "phy ioapic_data %p\n", get_phy_addr(ioapic_data));

    set_ioredtbl(2, 0, 0, 0x20);
    //set_ioredtbl(0, 0, 0, 0x20);
}

void lapic_send_eoi(){
    volatile u32 *eoi_reg = (u32 *)(lapic_base + LOCAL_APIC_EOI_REG);
    *eoi_reg = 0;
}

void apic_init(){
    disable_pic();
    lapic_init();
    ioapic_init();
}