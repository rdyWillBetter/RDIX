#include <rdix/task.h>
#include <rdix/kernel.h>
#include <rdix/device.h>
#include <common/interrupt.h>
#include <rdix/syscall.h>

void __idle(){
    /* 内核线程在创建时需要手动开中断 */
    set_IF(true);
    while (true){
        asm volatile(
            "sti\n" // 开中断
            "hlt\n" // 关闭 CPU，进入暂停状态，等待外中断的到来
        );
        set_IF(false);
        schedule();
        set_IF(true);
    }
}

#define __USER_LOG_INGO __LOG("[user mode]")
void __init(){
    /* 用户进程不许要手动开中断，详情见 kernel_to_user() */
    while (true){
        pid_t pid = fork();

        if (pid == 0){
            printf("child task, pid = %d, ppid = %d\n", getpid(), getppid());
            sleep(2000);
            exit(168);
        }
        else if (pid != 0){
            int32 status = -1;
            printf("parent task, pid = %d, ppid = %d\n", getpid(), getppid());
            if (waitpid(pid, &status) == -1){
                printf("no child process\n");
            }
            else{
                printf("waitpid success, status %d\n", status);
            }
        }
        while (true);
    }
}

void __keyboard(){
    set_IF(true);
    char ch;
    device_t *device = device_find(DEV_KEYBOARD, 0);

    while (true){
        device_read(device->dev, &ch, 1, 0, 0);
        printk("%c", ch);
    }
}

void __disk_test(){
    set_IF(true);
    disk_test();
    while (true);
}