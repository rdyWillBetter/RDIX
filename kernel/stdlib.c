#include <common/stdlib.h>

u8 bcd2bin(u8 bcd){
    return (bcd & 0xf) + (bcd >> 4) * 10;
}