#include <rdix/hba.h>
#include <rdix/pci.h>
#include <rdix/kernel.h>
#include <rdix/memory.h>
#include <common/assert.h>
#include <common/string.h>
#include <common/stdlib.h>

#define HBA_LOG_INFO __LOG("[hba]")
#define HBA_WARNING_INFO __WARNING("[hba warning]")

hba_t *hba;

const char* sata_spd[4] = {
    "Device not present",
    "SATA I",
    "SATA II",
    "SATA III",
};

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

    u32 cmd = read_register(hba->dev_info->bus, hba->dev_info->dev_num,\
                    hba->dev_info->function, PCI_CONFIG_SPACE_CMD);
    cmd |= ((1 << 1) | (1 << 10) | (1 << 2));
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
    /* 你复位个啥？ BIOS 好不容易初始化成功，你却复位他？ */
    //hba->io_base[REG_IDX(HBA_REG_GHC)] |= HBA_GHC_HR;
    /* 判断 hba 是否处于 AHCI 模式 */
    assert(hba->io_base[REG_IDX(HBA_REG_GHC)] & HBA_GHC_AE);
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

    while (1){
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

    /* 启动 hba 开始处理该端口对应命令链表 */
    port->reg_base[REG_IDX(HBA_PORT_PxCMD)] |= HBA_PORT_CMD_ST;
    
    return port;
}

void prase_deviceinfo(hba_dev_t* dev, u16 *data){

    memcpy(dev->sata_serial, data + 10, 20);
    for (int i = 0; i < 20; i += 2){
        char tmp = dev->sata_serial[i];
        dev->sata_serial[i] = dev->sata_serial[i + 1];
        dev->sata_serial[i + 1] = tmp;
    }
    dev->sata_serial[20] = '\0';
}

hba_dev_t* new_hba_device(hba_port_t *port, u8 spd){
    hba_dev_t *dev = (hba_dev_t *)malloc(sizeof(hba_dev_t));

    dev->spd = spd;
    dev->port = port;

    /* 不知为何在 VM 中将内存调到 256M 就会造成读取错误
     * 因为错误使用 sizeof */
    slot_num slot = load_ata_cmd(dev, ATA_CMD_IDENTIFY_DEVICE, 0, 0);
    dev->last_status = try_send_cmd(port, slot);

    prase_deviceinfo(dev, dev->data.ptr);

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
void build_cmd_head(cmd_list_slot *cmd_head, u8 CFL, u16 PRDTL, u32 base){
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
    //cmd_head->flag_rcbr = 0b0100;
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

    //item->i = 1;

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

    //***************************************
    if (cmd == ATA_CMD_IDENTIFY_DEVICE) {
        dev->data.ptr = malloc(1024);
        dev->data.size = 2;
    }

    cmd_list_slot *cmd_head = &port->vPxCLB[free_slot];
    cmd_tab_t *cmd_tab = (u32 *)malloc(sizeof(cmd_tab_t) + sizeof(cmd_tab_item));

    /* ==================================================
     * bug 调试记录
     * 在构建命令头的时候，CFL 的单位是 DW，也就是 CFL == 1 代表四个字节长度
     * 由于传入的时候没有除以 sizeof(u32) 导致 PxTFD.ERR 置一（任务文件错误）
     * ================================================== */
    build_cmd_head(cmd_head, sizeof(FIS_REG_H2D) / sizeof(u32), 1, (u32)get_phy_addr(cmd_tab));
    bulid_cmd_tab_fis((FIS_REG_H2D *)cmd_tab, cmd, startlba, count);
    build_cmd_tab_item(cmd_tab->item, dev->data.ptr, count);

    return free_slot;
}

send_status_t try_send_cmd(hba_port_t *port, slot_num slot){
    if (port->reg_base[REG_IDX(HBA_PORT_PxSIG)] != SATA_DEV_SIG)
        return NOT_SATA;

    time_t limit = 0x10000;
    for (; port->reg_base[REG_IDX(HBA_PORT_PxTFD)] & HBA_PORT_TFD_BSY; --limit);
    if (!limit)
        return _BUSY;

    port->reg_base[REG_IDX(HBA_PORT_PxCI)] |= 1 << slot;
    limit = 0x10000;
    for(; (port->reg_base[REG_IDX(HBA_PORT_PxCI)] & (1 << slot)) && limit; --limit);
    
    if (!limit){
        port->reg_base[REG_IDX(HBA_PORT_PxCI)] &= ~(1 << slot);
        return GENERAL_ERROR;
    }

    return SUCCESSFUL;
}

static char *error_str[4] = {
    "SUCCESS",
    "NOT_SATA",
    "BUSY",
    "GENERAL_ERROR",
};

void hba_init(){
    int dev_num = 0;
    /* 没有 hba 设备 */
    if (!probe_hba())
        return;

    hba_GHC_init();
    hba_devices_init();
    
    List_t *devices = hba->devices;
    
    for (ListNode_t *node = devices->end.next; node != &devices->end; node = node->next, ++dev_num){
        hba_dev_t *dev = (hba_dev_t *)node->owner;

        if (dev->last_status == SUCCESSFUL)
            printk(HBA_LOG_INFO "(device %d) sata serial_%s\n", dev_num, dev->sata_serial);

        else
            printk(HBA_LOG_INFO "(device %d) error code %s\n", dev_num, error_str[dev->last_status]);
    }
}