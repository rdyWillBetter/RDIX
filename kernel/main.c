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
#include <rdix/device.h>
#include <fs/fs.h>

//#define SYS_LOG_INFO "\033[1;35;40][system info]\033[0]\t"
#define SYS_LOG_INFO __LOG("[system]")

extern hba_t *hba;
void minix_init();
void xhc_init();

/* 当通过 rdix 自己的 loader 启动时 info 保存的是内存信息
 * 当通过 grub 启动时，info 保存的时 boot infomation */
void kernel_init(u32 magic, u32 info){
    device_init();
    console_init();
    /* printk("%x\n", *(u32*)info);
    while(true); */
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
    
    task_init();
    PCI_init();
    syscall_init();

    /* pci 设备的初始化可以做一个统一 */
    //hba_init();
    xhc_init();

    //buffer_init();
    //minix_init();
    /* 初始化完成后再启用 PCI 设备的总中断，开启总中断后就会积累中断 */
    //hba->io_base[REG_IDX(HBA_REG_GHC)] |= HBA_GHC_IE;
    // todo: 如果不存在 hba 设备，这里就会发生错误
    /* 开启外中断后才会进行调度 */
    set_IF(true);

    //disk_test();
}