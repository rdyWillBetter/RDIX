#include <rdix/hardware.h>
#include <rdix/memory.h>
#include <common/assert.h>
#include <rdix/kernel.h>
#include <rdix/memory.h>

#define ACPI_LOG_INFO __LOG("[acpi]")

/* lapic 寄存器是内存映射，通过内存访问，需要映射到虚拟地址空间
 * 位宽是 32bit */
volatile u8 *lapic_base;

/* ioapic 是 io 地址空间，分别有两个寄存器，一个是地址寄存器，一个是数据寄存器
 * 但是地址寄存器和数据寄存器是内存映射的，直接通过内存访问模式访问，需要重新映射到虚拟地址空间
 * 位宽是 32bit */
volatile u32 *ioapic_addr;
volatile u32 *ioapic_data;

/* irq 重定向表 */
struct {
    size_t size;
    u8 tab[16][2];
} irq_override_tab;

/* 参考 ACPI 文档 */
/* 总的寻找路径为 RSDP-> RSDT-> MADT-> I/O APIC Structure */
MADTStructure *_find_MADT(){
    ACPISDTHeader *RSDT = _find_RSDT();

    assert(RSDT != BIOS_MEM_SIZE);

    u32 entry_cnt = (RSDT->Length - sizeof(ACPISDTHeader)) / sizeof(ACPISDTHeader*);

    ACPISDTHeader **entry = (u32)RSDT + sizeof(ACPISDTHeader);

    for (int i = 0; i < entry_cnt; ++i){
        if (strcmp(&(entry[i]->Signature), "APIC", 4))
            return (MADTStructure *)entry[i];
    }

    return BIOS_MEM_SIZE;
}

/* 参考 APIC 文档 */
IOAPICStructure *_find_IOAPICS(){
    MADTStructure *MADT = _find_MADT();
    lapic_base = (u8 *)MADT->lapicAddr;

    u8* ics = (u8 *)((u32)MADT + 44);
    IOAPICStructure *ioapic;

    irq_override_tab.size = 0;

    while (ics < (u32)MADT + MADT->hd.Length){
        if (*ics == 1){        
            ioapic = (IOAPICStructure *)ics;
            ioapic_addr = (u32 *)ioapic->addr;
            ioapic_data = (u32 *)(ioapic->addr + 0x10); //数据寄存器位置比地址寄存器固定大 0x10
        }
        else if (*ics == 2){
            printk(ACPI_LOG_INFO "IRQ %d -> %d\n", ics[3], *((u32*)ics + 1));

            irq_override_tab.tab[irq_override_tab.size][0] = ics[3];
            irq_override_tab.tab[irq_override_tab.size][1] = *((u32*)ics + 1);
            
            ++irq_override_tab.size;
        }

        ics += ics[1];
    }

    return (IOAPICStructure *)ics;
}

void acpi_init(){
    _find_IOAPICS();

    /* 此时 lapic_base 为物理地址 */
    //assert(cpu_get_lapic_base() == (u32)lapic_base);

    printk(ACPI_LOG_INFO "lapic phy base 0x%p\n", lapic_base);
    printk(ACPI_LOG_INFO "ioapic io addr register base 0x%p\n", ioapic_addr);
    printk(ACPI_LOG_INFO "ioapic io data register base 0x%p\n", ioapic_data);
}