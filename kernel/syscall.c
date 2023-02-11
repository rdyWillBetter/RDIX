#include <rdix/syscall.h>

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

u32 test(){
    return _syscall0(SYS_NR_TEST);
}

void sleep(time_t ms){
    _syscall1(SYS_NR_SLEEP, ms);
}