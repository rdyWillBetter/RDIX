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

void handle_1(){
    char ch;
    while (true){
        keyboard_read(&ch, 1);
        printk("%c", ch);
    }
}

void user_1(){
    while (true){
        //printf("a\n");
    }
}

void handle_2(){
    char buf[10];
    while (true){
        keyboard_read(buf, 1);
        printk("[handle_2 get string] %s\n", buf);
    }
}

/* 当通过 rdix 自己的 loader 启动时 info 保存的是内存信息
 * 当通过 grub 启动时，info 保存的时 boot infomation */
void kernel_init(u32 magic, u32 info){
    console_init();
    
    if (magic == RDIX_MAGIC)
        printk("###Boot by RDIX LOADER###\n");
    else if (magic == MULTIBOOT_OS_MAGIC)
        printk("###Boot by MULTIBOOT2###\n");
    
    printk("test\n");

    IOAPICStructure *ioapic = _find_IOAPICS();

    gdt_init();
    mem_pg_init(magic,info);
    //task_init();
    interrupt_init();
    PCI_init();
    syscall_init();
    
    //task_create(handle_1, NULL, "test", 3, KERNEL_UID);
    //PCI_info();
    hba_init();
    //user_task_create(user_1, "user_1", 3);
    //task_create(handle_2, "test", 3, 0);
    
    set_IF(true);
}