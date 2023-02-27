#include <common/assert.h>
#include <rdix/kernel.h>
#include <common/type.h>

void assertion_failure(char *exp, char *file, char *base, int line){
    printk("file [%s]\nbase [%s]\nline [%d]\nerror: %s", file, base, line, exp);
    
    while (true);
}