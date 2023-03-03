#include <common/assert.h>
#include <rdix/kernel.h>
#include <common/type.h>
#include <common/console.h>

void assertion_failure(char *exp, char *file, char *base, int line){
    console_put_string("[assert]\n", WORD_TYPE_ASSERT);
    printk("file [%s]\nbase [%s]\nline [%d]\nerror: %s", file, base, line, exp);
    
    while (true);
}