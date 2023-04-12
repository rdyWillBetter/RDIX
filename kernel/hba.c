#include <rdix/hba.h>
#include <rdix/pci.h>
#include <rdix/kernel.h>
#include <rdix/memory.h>
#include <common/assert.h>
#include <common/string.h>
#include <common/stdlib.h>
#include <rdix/task.h>
#include <common/interrupt.h>
#include <rdix/syscall.h>
#include <rdix/device.h>

#define HBA_LOG_INFO __LOG("[hba]")
#define HBA_WARNING_INFO __WARNING("[hba warning]")

hba_t *hba;

const char* sata_spd[4] = {
    "Device not present",
    "SATA I",
    "SATA II",
    "SATA III",
};

slot_num load_ata_cmd(hba_dev_t *dev, u8 cmd, u64 startlba, u16 count);

send_status_t try_send_cmd(hba_port_t *port, slot_num slot);

bool probe_hba(){
    hba = (hba_t *)malloc(sizeof(hba_t));
    
    hba->devices = new_list();
    hba->io_base = NULL;
    hba->dev_info = NULL;

    /* 在 PCI 设备树中查找 hba 设备 */
    hba->dev_info = get_device_info(HBA_CC);

    /* 设备不存在 */
    if (hba->dev_info == NULL){
        printk(HBA_WARNING_INFO "no hba device!\n");
        return false;
    }

    /* 初始化 MSI 中断 */
    assert(__device_MSI_init(hba->dev_info, IRQ16_HBA + INTEL_INT_RESERVED) == 0);

    u32 cmd = read_register(hba->dev_info->bus, hba->dev_info->dev_num,\
                    hba->dev_info->function, PCI_CONFIG_SPACE_CMD);

    /* 启用 mmio 访问，启用设备间访问 */
    cmd |= __PCI_CS_CMD_MMIO_ENABLE | __PCI_CS_CMD_BUS_MASTER_ENABLE;

    write_register(hba->dev_info->bus, hba->dev_info->dev_num,\
                    hba->dev_info->function, PCI_CONFIG_SPACE_CMD, cmd);


    /* 存在 hba 设备 */
    /* 将 io 物理地址映射到虚拟内存空间 */
    hba->io_base = (u32 *)link_nppage(hba->dev_info->BAR[5].base_addr, hba->dev_info->BAR[5].size);
    
    phy_addr_t paddr = get_phy_addr(hba->io_base);

    return true;
}

void hba_GHC_init(){
    /* hba 版本 */
    u32 version = hba->io_base[REG_IDX(HBA_REG_VS)];
    u32 major = version >> 16;
    u32 minor = version & 0xffff;
    printk(HBA_LOG_INFO "hba version %x.%x\n", major, minor);

    /* =============================================
     * bug 调试记录
     * io_base[idx] 中 idx 是索引，索引的是 u32 类型。idx + 1 意味着要跨越四个字节
     * 而头文件中定义的都是他们的字节序，因此需要转换成 idx 才能找到正确的寄存器
     * ============================================= */
    /* 开始初始化 hba 全局寄存器 GHC */
    /* 复位 */
    /* 你复位个啥？ BIOS 好不容易初始化成功，你却复位他?
     * bios 会在 hba 的寄存器中帮你记录有哪些端口时可以使用的，初始化后这些信息就全没了 */
    //hba->io_base[REG_IDX(HBA_REG_GHC)] |= HBA_GHC_HR;
    /* 判断 hba 是否处于 AHCI 模式 */
    assert(hba->io_base[REG_IDX(HBA_REG_GHC)] & HBA_GHC_AE);
    /* 启用 HBA 全局中断 */
    hba->io_base[REG_IDX(HBA_REG_GHC)] |= HBA_GHC_IE;
    /* 清空中断暂挂 */
    hba->io_base[REG_IDX(HBA_REG_IS)] = -1;
    /* 每个端口命令列表中最多插槽 (slot) 数 */
    hba->per_port_slot_cnt = ((hba->io_base[REG_IDX(HBA_REG_CAP)] >> 8) & 0x1f) + 1;
}

hba_port_t *new_port(int32 port_num){
    hba_port_t *port = (hba_port_t *)malloc(sizeof(hba_port_t));

    port->port_num = port_num;

    port->reg_base = &hba->io_base[REG_IDX(HBA_PORT_BASE + HBA_PORT_SIZE * port_num)];
    
    /* stop port */
    port->reg_base[REG_IDX(HBA_PORT_PxCMD)] &= ~HBA_PORT_CMD_ST;
    port->reg_base[REG_IDX(HBA_PORT_PxCMD)] &= ~HBA_PORT_CMD_FRE;

    while (true){
        if (port->reg_base[REG_IDX(HBA_PORT_PxCMD)] & HBA_PORT_CMD_FR)
            continue;
        if (port->reg_base[REG_IDX(HBA_PORT_PxCMD)] & HBA_PORT_CMD_CR)
            continue;
        break;
    }

    /* 分配命令列表空间，后续使用时会清零 */
    port->vPxCLB = (u32 *)malloc(sizeof(cmd_list_slot) * hba->per_port_slot_cnt);

    /* 分配接收的 fis 存放空间 */
    port->vPxFB = (u32 *)malloc(sizeof(HBA_FIS));

    /* CI 位用于控制命令槽命令的发送 */
    port->reg_base[REG_IDX(HBA_PORT_PxCI)] = 0;

    port->reg_base[REG_IDX(HBA_PORT_PxCLB)] = get_phy_addr(port->vPxCLB);
    port->reg_base[REG_IDX(HBA_PORT_PxCLBU)] = 0;

    port->reg_base[REG_IDX(HBA_PORT_PxFB)] = get_phy_addr(port->vPxFB);
    port->reg_base[REG_IDX(HBA_PORT_PxFBU)] = 0;

    /* 错误状态寄存器清零 */
    port->reg_base[REG_IDX(HBA_PORT_PxSERR)] = -1;
    port->reg_base[REG_IDX(HBA_PORT_PxIS)] = -1;

    /* FIS receive enable */
    port->reg_base[REG_IDX(HBA_PORT_PxCMD)] |= HBA_PORT_CMD_FRE;

    /* 开中断 */
    port->reg_base[REG_IDX(HBA_PORT_PxIE)] |= HBA_PORT_IE_DPE;

    /* 启动 hba 开始处理该端口对应命令链表 */
    port->reg_base[REG_IDX(HBA_PORT_PxCMD)] |= HBA_PORT_CMD_ST;

    port->waiting_list = new_list();
    port->sending = NULL;
    port->last_status = -1;
    port->timer = NULL;
    
    return port;
}

void prase_deviceinfo(hba_dev_t* dev, u16 *data){

    memcpy(dev->sata_serial, data + 10, 20);
    for (int i = 0; i < 20; i += 2){
        swap(&dev->sata_serial[i], &dev->sata_serial[i + 1], 1);
    }
    dev->sata_serial[20] = '\0';
    
    memcpy(dev->model, data + 27, 40);
    for (int i = 0; i < 40; i += 2){
        swap(&dev->model[i], &dev->model[i + 1], 1);
    }
    dev->model[40] = '\0';

    /* 28bit LBA 下可访问的最大逻辑扇区数 */
    dev->max_lba = *(u32 *)(data + 60);

    /* 逻辑扇区大小 */
    dev->block_size = *(u32 *)(data + 117);

    /* 全球唯一标识符 */
    dev->wwn = *(u64 *)(data + 108);

    /* 每个物理扇区包含的逻辑扇区数 */
    dev->block_per_sec = 1 << (*(data + 106) & 0xf);

    if (!dev->block_size)
        dev->block_size = 512;

    /* 支持 48bit LBA 指令 */
    /* 具体参考 ACS-3/4.1.2/Table 7 */
    if (*(data + 83) & 0x400){
        /* 48bit LBA 下可访问的最大逻辑扇区数 */
        dev->max_lba = *((u64 *)(data + 100));

        if (*(data + 69) & 0x8)
            dev->max_lba = *((u64 *)(data + 230));

        dev->flags |= SATA_LBA_48_ENABLE;
    }
}

send_status_t sata_send_cmd(hba_dev_t *dev, SATA_CMD_TYPE cmd, u64 startlba, u16 count){

    slot_num slot = load_ata_cmd(dev, cmd, startlba, count);
    return try_send_cmd(dev->port, slot);
}

hba_dev_t* new_hba_device(hba_port_t *port, u8 spd){
    hba_dev_t *dev = (hba_dev_t *)malloc(sizeof(hba_dev_t));

    dev->spd = spd;
    dev->port = port;

    dev->flags = 0;

    dev->data = malloc(1024);

    /* 不知为何在 VM 中将内存调到 256M 就会造成读取错误。
     * 因为错误使用 sizeof */
    sata_send_cmd(dev, ATA_CMD_IDENTIFY_DEVICE, 0, 0);

    prase_deviceinfo(dev, dev->data);

    free(dev->data);

    return dev;
}

void hba_devices_init(){
    u32 port_enable = hba->io_base[REG_IDX(HBA_REG_PI)];
    List_t *devices = hba->devices;

    for (int port = 0; port < 32; ++port){
        /* 端口对应 PI 位有效 */
        if ((port_enable >> port) & 1){
            
            u32 pxssts = hba->io_base[REG_IDX(HBA_PORT_PxSSTS + HBA_PORT_BASE + HBA_PORT_SIZE * port)];
            
            u8 spd = (pxssts >> 4) & 0xf;
            /* 检测端口对应 ssts 寄存器 */
            if ((pxssts & 0x3 == 3) && spd){
                list_push(devices, new_listnode(new_hba_device(new_port(port), spd), 0));
            }
        }
    }
}

/* count 为传输扇区数 */
void bulid_cmd_tab_fis(FIS_REG_H2D *fis, u8 cmd, u64 startlba, u16 count){
    /* ===========================================
     * bug 调试记录
     * sizeof 最好跟类型名！不要跟变量，不然容易出错
     * 就是因为 sizeof(item)，导致内存清除不完全！调试了一个星期
     * 出错原因是 item 是一个指针变量 sizeof(item) == 4
     * =========================================== */
    memset(fis, 0, sizeof(FIS_REG_H2D));

    fis->fis_type = FIS_RH2D;
    /* 选项寄存器中 C 位有效代表该 fis 位命令更新 fis */
    fis->c = 1;

    fis->command = cmd;

    if (cmd == ATA_CMD_IDENTIFY_DEVICE)
        fis->device = 0;
    if (cmd == ATA_CMD_READ_DMA_EXT || cmd == ATA_CMD_READ_DMA || \
        cmd == ATA_CMD_WRITE_DMA_EXT || cmd == ATA_CMD_WRITE_DMA)
        fis->device = 1 << 6;

    fis->lba0 = (u8)startlba;
    fis->lba1 = (u8)(startlba >> 8);
    fis->lba2 = (u8)(startlba >> 16);
    fis->lba3 = (u8)(startlba >> 24);

    fis->lba4 = (u8)(startlba >> 32);
    fis->lba4 = (u8)(startlba >> 40);

    fis->countl = (u8)count;
    fis->counth = (u8)(count >> 8);
}

/* CFL 为 CFIS 长度，单位为 dw
 * PRDTL 为 PRDT 表中描述符个数，也就是 item 个数
 * base 为 cmmand table 基地址 */
void build_cmd_head(cmd_list_slot *cmd_head, u8 CFL, u16 PRDTL, u32 base, u8 cmd){
    /* ===========================================
     * bug 调试记录
     * sizeof 最好跟类型名！不要跟变量，不然容易出错
     * 就是因为 sizeof(item)，导致内存清除不完全！调试了一个星期
     * 出错原因是 item 是一个指针变量 sizeof(item) == 4
     * =========================================== */
    memset((void *)cmd_head, 0, sizeof(cmd_list_slot));

    assert(CFL <= 0x10);

    cmd_head->CFL = CFL;
    cmd_head->PRDTL = PRDTL;

    assert(!(base & 0x7f));

    cmd_head->CTBA = base >> 7;

    /* 设置后 hba 会自动清空 PxTFD.STS.BSY 和 PxCI 中的对应位 */
    /* 不要设置这里的 c 位，会导致数据接收失败！ */
    /* if (cmd == ATA_CMD_WRITE_DMA_EXT)
        cmd_head->flag_rcbr = 0b0100; */
    
    /* 如果是写操作，那么 W 标志位需要置位 */
    if (cmd == ATA_CMD_WRITE_DMA_EXT || cmd == ATA_CMD_WRITE_DMA)
        cmd_head->flag_pwa = 0b010;
}

/* count 为传输扇区数
 * 在读取和写入磁盘时需要设置，像发送 identify device 命令这种并不需要设置 count */
u16 *build_cmd_tab_item(cmd_tab_item *item, void *data, size_t count){
    /* ===========================================
     * bug 调试记录
     * sizeof 最好跟类型名！不要跟变量，不然容易出错
     * 就是因为 sizeof(item)，导致内存清除不完全！调试了一个星期
     * 出错原因是 item 是一个指针变量 sizeof(item) == 4
     * =========================================== */
    memset(item, 0, sizeof(cmd_tab_item));

    /* 一个扇区 512 字节 */
    size_t size = count << 9;

    /* data 必须是字对齐 */
    assert(!((u32)data & 1));
    item->dba = get_phy_addr(data);

    /* 最后一位必须是 1 */
    item->dbc = size - 1;

    /* 该条目传输完毕后产生中断 */
    item->i = 1;

    return data;
}

/* count 为扇区数 */
slot_num load_ata_cmd(hba_dev_t *dev, u8 cmd, u64 startlba, u16 count){
    slot_num free_slot = 32;
    hba_port_t *port = dev->port;

    for (int slot = 0; slot < 32; ++slot){
        if (!((port->reg_base[REG_IDX(HBA_PORT_PxCI)] >> slot) & 0x1)){
            free_slot = slot;
            break;
        }
    }

    /* 没找到空闲的插槽 */
    if (free_slot == 32)
        return 32;

    assert(dev->data != NULL);
    //***************************************
    /* if (cmd == ATA_CMD_IDENTIFY_DEVICE) {
        dev->data.ptr = malloc(1024);
        dev->data.size = 2;
    } */

    cmd_list_slot *cmd_head = &port->vPxCLB[free_slot];
    cmd_tab_t *cmd_tab = (u32 *)malloc(sizeof(cmd_tab_t) + sizeof(cmd_tab_item));

    /* ==================================================
     * bug 调试记录
     * 在构建命令头的时候，CFL 的单位是 DW，也就是 CFL == 1 代表四个字节长度
     * 由于传入的时候没有除以 sizeof(u32) 导致 PxTFD.ERR 置一（任务文件错误）
     * ================================================== */
    build_cmd_head(cmd_head, sizeof(FIS_REG_H2D) / sizeof(u32), 1, (u32)get_phy_addr(cmd_tab), cmd);
    bulid_cmd_tab_fis((FIS_REG_H2D *)cmd_tab, cmd, startlba, count);
    build_cmd_tab_item(cmd_tab->item, dev->data, count);

    return free_slot;
}

void sata_busy_wait(hba_port_t *port){
    if (port->reg_base[REG_IDX(HBA_PORT_PxTFD)] & HBA_PORT_TFD_BSY){
        /* 如果这里没通过，代表上一次发送出现问题后没有复位 */
        assert(port->sending != NULL);
        block(port->waiting_list, NULL, TASK_WAITING);
    }
}

void __timer(){
    set_IF(true);
    hba_port_t *port = NULL;

    /* 参数指针保存在 edi 中 */
    asm volatile(
        "movl %%edi,%0\n"
        :"=m"(port)
    );

    /* 超时计时器时长 */
    sleep(2000);

    port->last_status = GENERAL_ERROR;

    bool st = get_IF();
    set_IF(false);
    /* 中断迟迟不来，计时结束后需要通过 timer 来解除传输进程阻塞 */
    unblock(port->sending);

    /* 传输失败后要进行复位处理 */
    port->sending = NULL;

    /* kernel_thread_kill 不会去解除 waitpid 的阻塞，所里这里要关中断
     * 防止父进程先一步 waitpid */
    kernel_thread_kill(NULL);

    set_IF(st);
}

void sata_sending_wait(hba_port_t *port, slot_num slot){
    assert(port->sending == NULL);

    bool IF_stat = get_IF();
    set_IF(false);
    
    /* 清空中断状态寄存器 */
    port->reg_base[REG_IDX(HBA_PORT_PxIS)] = -1;

    /* 发送 */
    port->reg_base[REG_IDX(HBA_PORT_PxCI)] |= 1 << slot;

    port->sending = current_task();

    /* 开启超时计时器 */
    port->timer = task_create(__timer, port, "timer", 3, KERNEL_UID);

    block(NULL, NULL, TASK_BLOCKED);

    int32 *tmp;
    assert(sys_waitpid(((TCB_t*)port->timer->owner)->pid, &tmp) != -1);

    set_IF(IF_stat);
}

send_status_t try_send_cmd(hba_port_t *port, slot_num slot){
    
    if (port->reg_base[REG_IDX(HBA_PORT_PxSIG)] != SATA_DEV_SIG){
        port->last_status = NOT_SATA;
        return NOT_SATA;
    }
        
    if (current_task()){
        sata_busy_wait(port);

        port->last_status = GENERAL_ERROR;

        sata_sending_wait(port, slot);

        return port->last_status;
    }
    else {
        time_t limit = 0x100000;
        for (; port->reg_base[REG_IDX(HBA_PORT_PxTFD)] & HBA_PORT_TFD_BSY; --limit);

        if (!limit){
            port->last_status = _BUSY;
            return _BUSY;
        }
        
        /* 清空中断状态寄存器 */
        port->reg_base[REG_IDX(HBA_PORT_PxIS)] = -1;

        port->reg_base[REG_IDX(HBA_PORT_PxCI)] |= 1 << slot;

        limit = 0x100000;
        for(; (port->reg_base[REG_IDX(HBA_PORT_PxCI)] & (1 << slot)) && limit; --limit);

        if (!limit){
            port->reg_base[REG_IDX(HBA_PORT_PxCI)] &= ~(1 << slot);
            port->last_status = GENERAL_ERROR;
            return GENERAL_ERROR;
        }
    }

    port->last_status = SUCCESSFUL;
    return SUCCESSFUL;
}

static void hba_hander(u32 int_num){
    printk(HBA_LOG_INFO "in interrupt [0x%x]\n", int_num);
    
    List_t *devices = hba->devices;
    hba_port_t *port = NULL;

    for (ListNode_t *node = devices->end.next;
        node != &devices->end; node = node->next){

        hba_dev_t *dev = (hba_dev_t *)node->owner;
        
        /* 检测是哪个 port 发出的中断 */
        if (dev->port->reg_base[REG_IDX(HBA_PORT_PxIS)] & HBA_PORT_IS_DPS){
            port = dev->port;
            break;
        }
    }

    if (!port)
        PANIC("can not find a port which trigger the interrupt\n");
    
    assert(port->sending != NULL);

    /* 用于测试中断未到的情况 */
    //goto hba_hander_END;

    port->last_status = SUCCESSFUL;

    unblock(port->sending);
    port->sending = NULL;

    if (!list_isempty(port->waiting_list))
        unblock(port->waiting_list->end.next);

    if (port->timer)
        kernel_thread_kill(port->timer);

hba_hander_END:
    /* 清空端口中断状态寄存器，如果不清空，推出中断后 hba 会立马发出一个一模一样的中断，
     * 会造成二次中断的情况 */
    port->reg_base[REG_IDX(HBA_PORT_PxIS)] = -1;
    /* 清空 hba 中断暂挂，不清空的话 hba 不会再发送已暂挂端口的中断 */
    hba->io_base[REG_IDX(HBA_REG_IS)] = -1;

    lapic_send_eoi();
}

/* RorW == true 时为读操作
 * RorW == false 时为写操作 */
int __sata_io(hba_dev_t *dev, void *buffer, u64 startlba, size_t size, bool RorW){
    //printk("dev->max_lba = %x\n", dev->max_lba);
    assert(startlba < dev->max_lba);

    u8 cmd;

    /* 28lba 和 48lba 使用的是不同的指令 */
    if (dev->flags & SATA_LBA_48_ENABLE){
        cmd = RorW ? ATA_CMD_READ_DMA_EXT : ATA_CMD_WRITE_DMA_EXT;
    }
    else {
        cmd = RorW ? ATA_CMD_READ_DMA : ATA_CMD_WRITE_DMA;
    } 

    dev->data = buffer;

    sata_send_cmd(dev, cmd, startlba, size);

    if (dev->port->last_status == SUCCESSFUL)
        return 0;
    
    return 1;
}

/* size 单位为扇区 */
static int __sata_read_secs(hba_dev_t *dev, void *buffer, size_t count, u64 startlba){
    return __sata_io(dev, buffer, startlba, count, true);
}

static int __sata_write_secs(hba_dev_t *dev, void *buffer, size_t count, u64 startlba){
    return __sata_io(dev, buffer, startlba, count, false);
}

static int __sata_ioctl(hba_dev_t *dev, int cmd, void *args, int flags){
    printk("sata ioctl hasn't been implemented\n");

    return 0;
}

static char *error_str[4] = {
    "SUCCESS",
    "NOT_SATA",
    "BUSY",
    "GENERAL_ERROR",
};

char *test_str = "the user data shall be written to \n"\
            "non-volatile media before command completion is reported\n"\
            " regardless of whether or not volatile and/or \n"\
            "non-volatile write caching in the device is enabled.\n";

static void hba_ctrl_init(){
    /* 没有 hba 设备 */
    if (!probe_hba())
        return;

    hba_GHC_init();
    hba_devices_init();
}

static void sata_install(){
    int dev_num = 0;
    List_t *devices = hba->devices;

    for (ListNode_t *node = devices->end.next; node != &devices->end; node = node->next, ++dev_num){
        hba_dev_t *dev = (hba_dev_t *)node->owner;

        if (dev->port->last_status == SUCCESSFUL){
            printk(HBA_LOG_INFO "(device %d) sata serial_%s\n", dev_num, dev->sata_serial);
            printk(HBA_LOG_INFO "(device %d) sata model_%s\n", dev_num, dev->model);
            printk(HBA_LOG_INFO "(device %d) sata maxLBA_%x\n", dev_num, dev->max_lba);
            printk(HBA_LOG_INFO "(device %d) sata blocksize_%d\n", dev_num, dev->block_size);
            printk(HBA_LOG_INFO "(device %d) sata blockPerSec_%d\n", dev_num, dev->block_per_sec);
            if (dev->flags & SATA_LBA_48_ENABLE)
                printk (HBA_LOG_INFO "(device %d) support 48 LBA\n", dev_num);
            else
                printk(HBA_LOG_INFO "(device %d) do not support 48 LBA\n", dev_num);

            device_install(DEV_BLOCK, DEV_SATA_DISK, dev, dev->sata_serial, 0,
                        __sata_ioctl, __sata_read_secs, __sata_write_secs);
        }
        else
            printk(HBA_LOG_INFO "(device %d) error code %s\n", dev_num, error_str[dev->port->last_status]);
    }
}

void hba_init(){
    hba_ctrl_init();
    sata_install();

    /* 安装中断前需要在 MSI 中设置中断向量值 */
    install_int(IRQ16_HBA, 0, 0, hba_hander);
}

void disk_test(){
    // 尝试读取扇区
    void *buf = malloc(1 << 9);

    device_t *dev = device_find(DEV_SATA_DISK, 0);

    /* if (device_write(dev->dev, test_str, 1, 0x7000, 0) == 0){
        printk(HBA_LOG_INFO "disk write success\n"); 
    }
    else{
        printk("write error status: %s\n", error_str[((hba_dev_t *)dev->ptr)->port->last_status]);
    } */

    /* printk(HBA_LOG_INFO "before read disk\n");
    mdebug(buf, 64); */

    if (device_read(dev->dev, buf, 1, 0x7000, 0) == 0){
            printk(HBA_LOG_INFO "disk read success\n");
            mdebug(buf, 64);
    }
    else{
        printk("read error status: %s\n", error_str[((hba_dev_t *)dev->ptr)->port->last_status]);
    }
}