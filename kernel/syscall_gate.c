#include <common/interrupt.h>
#include <rdix/kernel.h>
#include <rdix/syscall.h>
#include <rdix/task.h>
#include <common/console.h>
#include <rdix/memory.h>

#define SYSCALL_NUM 64

typedef u32 (*syscall_gate_t)(u32 ebx, u32 ecx, u32 edx, u32 sys_vector);

/* int 0x80 将根据保存在 eax 中的调用号，调用syscall_table中的函数 */
syscall_gate_t syscall_table[SYSCALL_NUM];

void syscall_check(u32 sys_vector){
    if (sys_vector >= SYSCALL_NUM)
        PANIC("syscall error: no such func\n");
}

static u32 syscall_default_handle(){
    printk("syscall not implemented\n");
    return 0;
}

static u32 sys_test(u32 ebx, u32 ecx, u32 edx, u32 sys_vector){
    BMB;
    link_page(0x1600000);
    BMB;
    char *p = (char*)0x1600000;
    p[2] = 'X';
    BMB;
    unlink_page(0x1600000);
    BMB;
    
}

static int32 sys_write(fd_t fd, char *buf, u32 len, u32 sys_vector){
    if (fd == stdout || fd == stderr){
        console_put_string(buf);
        return 0;
    }

    return -1;
}

void syscall_init(){

    for (int i = 0; i < SYSCALL_NUM; ++i){
        syscall_table[i] = syscall_default_handle;
    }
    
    syscall_table[SYS_NR_TEST] = (syscall_gate_t)sys_test;
    syscall_table[SYS_NR_SLEEP] = (syscall_gate_t)task_sleep;
    syscall_table[SYS_NR_WRITE] = (syscall_gate_t)sys_write;
}