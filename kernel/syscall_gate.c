#include <common/interrupt.h>
#include <rdix/kernel.h>
#include <rdix/syscall.h>
#include <rdix/task.h>

#define SYSCALL_NUM 64

/* int 0x80 将根据保存在 eax 中的调用号，调用syscall_table中的函数 */
u32 (*syscall_table[SYSCALL_NUM])(u32 ebx, u32 ecx, u32 edx, u32 sys_vector);

void syscall_check(u32 sys_vector){
    if (sys_vector >= SYSCALL_NUM)
        PANIC("syscall error: no such func\n");
}

static u32 syscall_default_handle(){
    printk("syscall not implemented\n");
    return 0;
}

static u32 sys_test(u32 ebx, u32 ecx, u32 edx, u32 sys_vector){
    printk("ebx = %#d\necx = %#d\nedx = %#d\nsys_vector = %#p\n",\
            ebx, ecx, edx, sys_vector);
    return 213;
}

void syscall_init(){

    for (int i = 0; i < SYSCALL_NUM; ++i){
        syscall_table[i] = syscall_default_handle;
    }
    
    syscall_table[SYS_NR_TEST] = sys_test;
    syscall_table[SYS_NR_SLEEP] = task_sleep;
}