#include <rdix/syscall.h>

/* eax 指明调用 0x80 中第几个系统调用函数
 * ebx 为第一个参数
 * ecx 为第二个参数
 * edx 为第三个参数 */
static _inline u32 _syscall0(u32 nr){
    u32 ret;
    asm volatile(
        "int $0x80\n"
        :"=a"(ret)
        :"a"(nr)
    );
    return ret;
}

static _inline u32 _syscall1(u32 nr, u32 arg1){
    u32 ret;
    asm volatile(
        "int $0x80\n"
        :"=a"(ret)
        :"a"(nr),"b"(arg1)
    );
    return ret;
}

static _inline u32 _syscall2(u32 nr, u32 arg1, u32 arg2){
    u32 ret;
    asm volatile(
        "int $0x80\n"
        :"=a"(ret)
        :"a"(nr),"b"(arg1),"c"(arg2)
    );
    return ret;
}

static _inline u32 _syscall3(u32 nr, u32 arg1, u32 arg2, u32 arg3){
    u32 ret;
    asm volatile(
        "int $0x80\n"
        :"=a"(ret)
        :"a"(nr),"b"(arg1),"c"(arg2),"d"(arg3)
    );
    return ret;
}


u32 test(){
    return _syscall0(SYS_NR_TEST);
}

void sleep(time_t ms){
    _syscall1(SYS_NR_SLEEP, ms);
}

int32 write(fd_t fd, char *buf, size_t len){
    return (int32)_syscall3(SYS_NR_WRITE, fd, (u32)buf, len);
}

int32 brk(void *vaddr){
    return (int32)_syscall1(SYS_NR_BRK, vaddr);
}

pid_t fork(){
    return _syscall0(SYS_NR_FORK);
}

pid_t getpid(){
    return _syscall0(SYS_NR_GETPID);
}

pid_t getppid(){
    return _syscall0(SYS_NR_GETPPID);
}

pid_t exit(int status){
    return _syscall1(SYS_NR_EXIT, status);
}

pid_t waitpid(pid_t pid, int32 *status){
    return _syscall2(SYS_NR_WAITPID, pid, status);
}

void yield(){
    return _syscall0(SYS_NR_YIELD);
}