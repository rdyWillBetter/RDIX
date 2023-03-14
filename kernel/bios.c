#include <common/string.h>
#include <rdix/hardware.h>
#include <rdix/memory.h>
#include <common/assert.h>
#include <rdix/kernel.h>

static bool _REDP_checksum(RSDPDes_t *RSDP){
    char *p = (char *)RSDP;
    u8 sum = 0;

    for (int i = 0; i < 20; ++i){
        sum += p[i];
    }

    return sum == 0 ? true : false;
}

RSDPDes_t *_find_RSDP(){
    for (char *ptr = 0xE0000; ptr < BIOS_MEM_SIZE; ptr += 16){
        if ((u32)(*(u32 *)ptr) == RSDP_SIG_L && _REDP_checksum(ptr)){
            return ptr;
        }
    }
    return BIOS_MEM_SIZE;
}

ACPISDTHeader *_find_RSDT(){
    RSDPDes_t *RSDP = _find_RSDP();

    if (RSDP != BIOS_MEM_SIZE){
        return RSDP->RsdtAddress;
    }

    return BIOS_MEM_SIZE;
}