#include <common/stdlib.h>
#include <rdix/kernel.h>
#include <common/assert.h>

u8 bcd2bin(u8 bcd){
    return (bcd & 0xf) + (bcd >> 4) * 10;
}

void mdebug(void *data, size_t count){
    size_t col = 8;
    size_t row = count / col + (count % col + col - 1) / col;
    char *ptr = (char *)data;
    
    for (int i = 0; i < row; ++i){
        for (int j = 0; j < col; ++j){
            printk("%x\t", ptr[i * col + j] & 0xff);
        }
        printk("\n");
    }
}