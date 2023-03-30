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
    
    printk("\t\t\t");
    for (int i = 0; i < col; ++i){
        printk("\033[0;33;40]%02X\t", i);
    }
    printk("\n");

    for (int i = 0; i < row; ++i){
        printk("\033[0;33;40]%08X:\033[0]\t", data + i * col);

        for (int j = 0; j < col; ++j){
            printk("%02X\t", ptr[i * col + j] & 0xff);
        }
        printk("|  ");

        for (int j = 0; j < col; ++j){
            char ch = ptr[i * col + j] & 0xff;

            if (ch < 32 || ch > 126)
                ch = '.';
            
            printk("%c", ch);
        }

        printk("\n");
    }
}

void swap(void *a, void *b, size_t size){
    u8 tmp;
    u8 *pa = a, *pb = b;

    for (int i = 0; i < size; ++i, ++pa, ++pb){
        tmp = *pa;
        *pa = *pb;
        *pb = tmp;
    }
}