#include <rdix/kernel.h>
#include <common/io.h>
#include <common/console.h>
#include <common/string.h>
#include <common/global.h>
#include <common/interrupt.h>
#include <common/time.h>
#include <rdix/memory.h>
#include <rdix/task.h>
#include <rdix/syscall.h>
#include <rdix/multiboot2.h>
#include <common/list.h>
#include <common/stdio.h>
#include <rdix/pci.h>
#include <rdix/hba.h>
#include <rdix/hardware.h>

//#define SYS_LOG_INFO "\033[1;35;40][system info]\033[0]\t"
#define SYS_LOG_INFO __LOG("[system]")

void handle_1(){
    char ch;
    while (true){
        keyboard_read(&ch, 1);
        printk("%c", ch);
    }
}

void user_1(){
    int i = 0;
    while (true){
        ++i;
        if (i < 10)
            printf("%d\n", i);
    }
}

void handle_2(){
    int i = 0;
    while (true){
        ++i;
        if (i < 10)
            printf("%d\n", i);
    }
}

/* 当通过 rdix 自己的 loader 启动时 info 保存的是内存信息
 * 当通过 grub 启动时，info 保存的时 boot infomation */
void kernel_init(u32 magic, u32 info){
    console_init();
    
    if (magic == RDIX_MAGIC)
        printk(SYS_LOG_INFO "Boot by RDIX LOADER\n");
    else if (magic == MULTIBOOT_OS_MAGIC)
        printk(SYS_LOG_INFO "Boot by MULTIBOOT2\n");

    gdt_init();

    /* acpi 的初始化必须放在分页模式开启之前，因为 apci 中的
     * 结构体很有可能存在于后 2G 的物理地址。
     * acpi_init 只要记录需要用到的寄存器物理地址就行了 */
    acpi_init();
    mem_pg_init(magic,info);
    interrupt_init();
    
    BMB;
    task_init();
    PCI_init();
    syscall_init();
    
    //task_create(handle_1, NULL, "k0", 3, KERNEL_UID);
    //PCI_info();
    hba_init();
    //user_task_create(user_1, "user_1", 3);
    task_create(handle_2, NULL, "test", 3, 0);
    
    set_IF(true);
}