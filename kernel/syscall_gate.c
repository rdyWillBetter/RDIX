#include <common/interrupt.h>
#include <rdix/kernel.h>
#include <rdix/syscall.h>
#include <rdix/task.h>
#include <common/console.h>
#include <rdix/memory.h>
#include <rdix/device.h>

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
        device_t *dev = device_find(DEV_CONSOLE, 0);
        device_write(dev->dev, buf, 0, 0, 0);
        
        return 0;
    }

    return -1;
}

extern int32 sys_brk(vir_addr_t vaddr);
extern pid_t sys_fork();
extern pid_t sys_getpid();
extern pid_t sys_getppid();
extern void sys_exit(int status);
extern pid_t sys_waitpid(pid_t pid, int32 *status);
extern void sys_yield();

void syscall_init(){

    for (int i = 0; i < SYSCALL_NUM; ++i){
        syscall_table[i] = syscall_default_handle;
    }
    
    syscall_table[SYS_NR_TEST] = (syscall_gate_t)sys_test;
    syscall_table[SYS_NR_SLEEP] = (syscall_gate_t)task_sleep;
    syscall_table[SYS_NR_WRITE] = (syscall_gate_t)sys_write;
    syscall_table[SYS_NR_BRK] = (syscall_gate_t)sys_brk;
    syscall_table[SYS_NR_FORK] = (syscall_gate_t)sys_fork;
    syscall_table[SYS_NR_GETPID] = (syscall_gate_t)sys_getpid;
    syscall_table[SYS_NR_GETPPID] = (syscall_gate_t)sys_getppid;
    syscall_table[SYS_NR_EXIT] = (syscall_gate_t)sys_exit;
    syscall_table[SYS_NR_WAITPID] = (syscall_gate_t)sys_waitpid;
    syscall_table[SYS_NR_YIELD] = (syscall_gate_t)sys_yield;
}