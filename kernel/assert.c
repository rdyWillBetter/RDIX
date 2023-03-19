#include <common/assert.h>
#include <rdix/kernel.h>
#include <common/type.h>
#include <common/console.h>

void assertion_failure(char *exp, char *file, char *base, int line){
    printk("\033[0]\33[1;30;41][assert]\033[0]"\
            "file [%s]\nbase [%s]\nline [%d]\nerror: %s", file, base, line, exp);
    
    while (true);
}