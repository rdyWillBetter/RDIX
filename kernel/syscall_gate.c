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

extern int32 sys_brk(vir_addr_t vaddr);
extern pid_t sys_fork();
extern pid_t sys_getpid();
extern pid_t sys_getppid();
extern void sys_exit(int status);
extern pid_t sys_waitpid(pid_t pid, int32 *status);
extern void sys_yield();
extern fd_t sys_open(char *filename, int flags, int mode);
extern fd_t sys_create(char *filename, int mode);
extern void sys_close(fd_t fd);
extern int sys_read(fd_t fd, char *buf, int count);
extern int sys_write(fd_t fd, char *buf, int count);
extern int sys_lseek(fd_t fd, idx_t offset, whence_t whence);
extern int sys_readdir(fd_t fd, dir_entry *dir, u32 count);
extern int sys_mkdir(const char *pathname, int mode);
extern int sys_rmdir(const char *pathname);
extern int sys_chdir(char *pathname);
void sys_getpwd(char *buf, size_t len);
int sys_link(char *oldname, char *newname);
int sys_unlink(char *pathname);

void syscall_init(){

    for (int i = 0; i < SYSCALL_NUM; ++i){
        syscall_table[i] = syscall_default_handle;
    }
    
    syscall_table[SYS_NR_TEST] = (syscall_gate_t)sys_test;
    syscall_table[SYS_NR_SLEEP] = (syscall_gate_t)task_sleep;
    syscall_table[SYS_NR_BRK] = (syscall_gate_t)sys_brk;
    syscall_table[SYS_NR_FORK] = (syscall_gate_t)sys_fork;
    syscall_table[SYS_NR_GETPID] = (syscall_gate_t)sys_getpid;
    syscall_table[SYS_NR_GETPPID] = (syscall_gate_t)sys_getppid;
    syscall_table[SYS_NR_EXIT] = (syscall_gate_t)sys_exit;
    syscall_table[SYS_NR_WAITPID] = (syscall_gate_t)sys_waitpid;
    syscall_table[SYS_NR_YIELD] = (syscall_gate_t)sys_yield;
    syscall_table[SYS_NR_OPEN] = (syscall_gate_t)sys_open;
    syscall_table[SYS_NR_CREATE] = (syscall_gate_t)sys_create;
    syscall_table[SYS_NR_CLOSE] = (syscall_gate_t)sys_close;
    syscall_table[SYS_NR_READ] = (syscall_gate_t)sys_read;
    syscall_table[SYS_NR_WRITE] = (syscall_gate_t)sys_write;
    syscall_table[SYS_NR_SEEK] = (syscall_gate_t)sys_lseek;
    syscall_table[SYS_NR_READDIR] = (syscall_gate_t)sys_readdir;
    syscall_table[SYS_NR_MKDIR] = (syscall_gate_t)sys_mkdir;
    syscall_table[SYS_NR_RMDIR] = (syscall_gate_t)sys_rmdir;
    syscall_table[SYS_NR_CHDIR] = (syscall_gate_t)sys_chdir;
    syscall_table[SYS_NR_GETPWD] = (syscall_gate_t)sys_getpwd;
    syscall_table[SYS_NR_LINK] = (syscall_gate_t)sys_link;
    syscall_table[SYS_NR_UNLINK] = (syscall_gate_t)sys_unlink;
}