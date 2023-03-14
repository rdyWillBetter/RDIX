#include <rdix/hardware.h>
#include <rdix/memory.h>
#include <common/assert.h>
#include <rdix/kernel.h>

ACPISDTHeader *_find_MADT(){
    ACPISDTHeader *RSDT = _find_RSDT();

    assert(RSDT != BIOS_MEM_SIZE);

    u32 entry_cnt = (RSDT->Length - sizeof(ACPISDTHeader)) / sizeof(ACPISDTHeader*);

    ACPISDTHeader **entry = (u32)RSDT + sizeof(ACPISDTHeader);

    for (int i = 0; i < entry_cnt; ++i){
        if (strcmp(&(entry[i]->Signature), "APIC", 4))
            return entry[i];
    }

    return BIOS_MEM_SIZE;
}

IOAPICStructure *_find_IOAPICS(){
    ACPISDTHeader *MADT = _find_MADT();

    u8* ics = (u8 *)((u32)MADT + 44);
    IOAPICStructure *ioapic = 0;

    while (ics < (u32)MADT + MADT->Length){
        if (*ics == 1){
            ioapic = (IOAPICStructure *)ics;
            printk("[APIC] I/O APIC Address 0x%x\n", ioapic->addr);
        }
        else if (*ics == 2){
            printk("[APIC] IRQ %d -> %d\n", ics[3], *((u32*)ics + 1));
        }

        ics += ics[1];
    }

    return (IOAPICStructure *)ics;
}