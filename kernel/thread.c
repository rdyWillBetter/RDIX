#include <rdix/task.h>
#include <rdix/kernel.h>
#include <rdix/device.h>
#include <common/interrupt.h>
#include <rdix/syscall.h>
#include <common/stdlib.h>
#include <fs/fs.h>
#include <common/string.h>

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

int osh_main();
#define __USER_LOG_INGO __LOG("[user mode]")
void __init(){
    
    /* 用户进程不许要手动开中断，详情见 kernel_to_user() */
    //while (true){
        /* pid_t pid = fork();

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
        while (true); */
    //}

    /* pid_t pid = fork();
    int status = -1;

    if (pid){
        pid_t t = 0;
        t = waitpid(pid, &status);
        printf("pid_%d, exit code %d", t, status);
    }      
    else
        osh_main(); */
    
    while (true);
}

#include <rdix/xhci.h>
extern xhc_t* xhc;
extern u64 *debug_pba;
void __usb_test(){
    asm volatile("sti");
    int_reg_set *int_set = (int_reg_set *)((u32)xhc->run_base + 0x20);
    u32 *data = int_set[0].ERDP_Lo & ~0x3f;
    //u32 *data_hi = int_set[0].ERDP_Hi;
    
    /* for (int i = 0; i < 7; ++i){
        xhc->op_base[0x100 + 4 * i] |= 0x10;
    }
 */
    while (true){
        
        //printk("int_set[0].ERDP_Lo = %x\n", int_set[0].ERDP_Lo);
        //printk("IMAN_%x| ", int_set[0].IMAN);
        
        //printk("ERDP_Lo_%x |IMAN_%x| EHB_%x\n", int_set[0].ERDP_Lo, int_set[0].IMAN, int_set[0].ERDP_Lo & 0x3f);
        //printk("EHB_%x\n", int_set[0].ERDP_Lo & 0xf);
       /*  for (int i = 0; i < 7; ++i){
            printk("\033[0;33;40]p%d\033[0]_%x| ", i,xhc->op_base[0x100 + 4 * i]);
        } */

/*         for (int i = 0; i < 4; ++i)
            printk("\033[0;33;40]D%d\033[0] %x %x %x %x| ", i, data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]);
 */
        //printk("\n");

        //data[3] &= ~1;
        //int_set[0].ERDP_Lo = (u32)data | 8;
        //int_set[0].ERDP_Hi = 0;
        //while (true);
        //sleep(3000);
    }
}

/* void __keyboard(){
    set_IF(true);
    char ch;
    device_t *device = device_find(DEV_KEYBOARD, 0);
    assert(device);

    while (true){
        device_read(device->dev, &ch, 1, 0, 0);
        printk("%c", ch);
    }
} */

#define PART_LOG_INFO __LOG("[part test]")
void __test(idx_t idx){
    device_t *part0 = device_find(DEV_DISK_PART, 0);
    super_block_t *sb = get_super(part0->dev);

    printk("\tinodes\t%d\n", sb->desc->inodes);
    printk("\tzones\t%d\n", sb->desc->zones);
    printk("\timap_blocks\t%d\n", sb->desc->imap_blocks);
    printk("\tzmap_blocks\t%d\n", sb->desc->zmap_blocks);
    printk("\tfirstdatazone\t%d\n", sb->desc->firstdatazone);
    printk("\tlog_zone_size\t%d\n", sb->desc->log_zone_size);
    printk("\tmax_size\t%d\n", sb->desc->max_size);
    printk("\tmagic\t%x\n", sb->desc->magic);

    /* inode_t inode = ialloc(part0->dev);
    ialloc(part0->dev);
    ialloc(part0->dev);
    ialloc(part0->dev); */

    m_inode *dir = iget(part0->dev, 1);
    m_inode *node = namei("hello");

    m_inode *lk_test1 = namei("/lk_test1");
    m_inode *test1 = namei("/test1");
    m_inode *hello = namei("/hello");
    m_inode *test2 = namei("/hello/test2");

    //inode_truncate(node);

    //assert(sys_mkdir("/hello", 0xffff) == 0);
    //assert(sys_rmdir("/he") == 0);
    //assert(sys_link("/test1", "/lk_test1") == 0);
    //assert(sys_unlink("/lk_test3") == 0);
    //assert(sys_unlink("/he/test2") == 0);
    //assert(sys_unlink("/test6") == 0);
    //assert(sys_unlink("/hello") == 0);

    /* fd_t fk = EOF;
    char data[16] = "hello by write\n"; 
    assert((fk = open("hello/fk", O_TRUNC | O_RDWR, 0777)) != EOF);
    write(fk, data, 16); */

    chdir("/hello");

    sync_dev(dir->dev);
    printk("\tinode mode\t%d\n", dir->desc->mode);
}

void _test2(){
    
    while (true);
}

void __disk_test(){
    set_IF(true);
    _test2();
    while (true);
}

void __disk_test2(){
    set_IF(true);
    __test(1);
    while (true);
}