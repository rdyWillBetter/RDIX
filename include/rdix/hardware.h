#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#include <common/type.h>

#define RSDP_SIG_L 0x20445352

typedef struct RSDPDescriptor {
    char Signature[8];
    u8 Checksum;
    char OEMID[6];
    u8 Revision;
    u32 RsdtAddress;
} _packed RSDPDes_t;

typedef struct ACPISDTHeader {
    char Signature[4];
    u32 Length;
    u8 Revision;
    u8 Checksum;
    char OEMID[6];
    char OEMTableID[8];
    u32 OEMRevision;
    u32 CreatorID;
    u32 CreatorRevision;
    //u32 lapicAddr;
    //u32 Flags;
} _packed ACPISDTHeader;

typedef struct MADTStructure{
    ACPISDTHeader hd;
    u32 lapicAddr;
    u32 flags;
} MADTStructure;

typedef struct IOAPICStructure{
    u8 type;
    u8 length;
    u8 id;
    u8 reserved;
    u32 addr;
    u32 GSIBase;
} _packed IOAPICStructure;

RSDPDes_t *_find_RSDP();
ACPISDTHeader *_find_RSDT();
MADTStructure *_find_MADT();

void acpi_init();

bool cpuHasMSR();
void cpuGetMSR(u32 msr, u32 *lo, u32 *hi);
void cpuSetMSR(u32 msr, u32 lo, u32 hi);

#endif