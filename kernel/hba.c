#include <rdix/hba.h>
#include <rdix/pci.h>
#include <rdix/kernel.h>
#include <rdix/memory.h>
#include <common/assert.h>
#include <common/string.h>
#include <common/stdlib.h>

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
        printk("[warning] no hba device!\n");
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
    printk("vbase_%x, pbase_%x\n", hba->io_base, paddr);

    return true;
}

void hba_GHC_init(){
    /* hba 版本 */
    u32 version = hba->io_base[REG_IDX(HBA_REG_VS)];
    u32 major = version >> 16;
    u32 minor = version & 0xffff;
    printk("[hba] hba version %x.%x\n", major, minor);

    /* =============================================
     * bug 调试记录
     * io_base[idx] 中 idx 是索引，索引的是 u32 类型。idx + 1 意味着要跨越四个字节
     * 而头文件中定义的都是他们的字节序，因此需要转换成 idx 才能找到正确的寄存器
     * ============================================= */
    /* 开始初始化 hba 全局寄存器 GHC */
    /* 复位 */
    //hba->io_base[REG_IDX(HBA_REG_GHC)] |= HBA_GHC_HR;
    /* 启用 AHCI 模式而非 IDE 模式 */
    assert(hba->io_base[REG_IDX(HBA_REG_GHC)] & HBA_GHC_AE);
    /* 启用中断，接受来自端口的中断 */
    //hba->io_base[REG_IDX(HBA_REG_GHC)] |= HBA_GHC_IE;
    /* 每个端口命令列表中最多插槽数 */
    hba->per_port_slot_cnt = ((hba->io_base[REG_IDX(HBA_REG_CAP)] >> 8) & 0x1f) + 1;
}

hba_port_t *new_port(int32 port_num){
    hba_port_t *port = (hba_port_t *)malloc(sizeof(hba_port_t));

    port->port_num = port_num;

    port->reg_base = &hba->io_base[REG_IDX(HBA_PORT_BASE + HBA_PORT_SIZE * port_num)];
    printk("port base virtual memery: %x\n", port->reg_base);
    printk("port base physical memery: %x\n", get_phy_addr(port->reg_base));
    
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

    /* 分配命令列表空间 */
    port->vPxCLB = (u32 *)malloc(sizeof(cmd_list_slot) * hba->per_port_slot_cnt);
    //port->vPxCLB = 0x300000 + (port_num << 10);
    //memset(port->vPxCLB, 0, 1024);
    /* 分配接收的 fis 存放空间 */
    port->vPxFB = (u32 *)malloc(sizeof(u32) * 64);
    //port->vPxFB = 0x300000 + (port_num << 8) + (32 << 10);
    //memset(port->vPxFB, 0, sizeof(HBA_FIS));
    /* CI 位用于控制命令槽命令的发送 */
    port->reg_base[REG_IDX(HBA_PORT_PxCI)] = 0;

    port->reg_base[REG_IDX(HBA_PORT_PxCLB)] = get_phy_addr(port->vPxCLB);
    port->reg_base[REG_IDX(HBA_PORT_PxCLBU)] = 0;

    port->reg_base[REG_IDX(HBA_PORT_PxFB)] = get_phy_addr(port->vPxFB);
    port->reg_base[REG_IDX(HBA_PORT_PxFBU)] = 0;

    /* 错误状态寄存器清零 */
    port->reg_base[REG_IDX(HBA_PORT_PxSERR)] = -1;
    port->reg_base[REG_IDX(HBA_PORT_PxIS)] = -1;

    while (port->reg_base[REG_IDX(HBA_PORT_PxCMD)] & HBA_PORT_CMD_CR);
    port->reg_base[REG_IDX(HBA_PORT_PxCMD)] |= HBA_PORT_CMD_FRE;

    u8 PxTFD_STS = port->reg_base[REG_IDX(HBA_PORT_PxTFD)];
    if ((PxTFD_STS & 0x80) || (PxTFD_STS & 8) || (PxTFD_STS & 1)){
        printk("initial fail\n");
        while(true);
    }
    printk("initial successed\n");

    port->reg_base[REG_IDX(HBA_PORT_PxCMD)] &= ~1;
    /* 启动 hba 开始处理该端口对应命令链表 */
    printk("before:num_%d, cmd_%x\n", port_num, port->reg_base[REG_IDX(HBA_PORT_PxCMD)]);
    port->reg_base[REG_IDX(HBA_PORT_PxCMD)] |= 1;
    printk("after:num_%d, cmd_%x\n", port_num, port->reg_base[REG_IDX(HBA_PORT_PxCMD)]);

    
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

    /* 不知为何在 VM 中将内存调到 256M 就会造成读取错误 */
    u16 *data = NULL;
    slot_num slot = load_ata_cmd(port, &data, 0xec, 0, 0);

    printk("[data berfore]\n");
    mdebug(data, 40);

    dev->last_status = try_send_cmd(port, slot);

    printk("[data after]\n");
    mdebug(data, 40);

    prase_deviceinfo(dev, data);

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

void bulid_cmd_tab_fis(FIS_REG_H2D *fis, u8 cmd, u64 lba, u16 count){
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
    fis->device = 0;

    /*
    fis->lba_l = lba & 0xffffff;
    fis->lba_h = (lba >> 24) & 0xffffff;

    fis->count = count;
    */
}

void build_cmd_head(cmd_list_slot *cmd_head, u8 CFL, u16 PRDTL, u32 base){
    /* ===========================================
     * bug 调试记录
     * sizeof 最好跟类型名！不要跟变量，不然容易出错
     * 就是因为 sizeof(item)，导致内存清除不完全！调试了一个星期
     * 出错原因是 item 是一个指针变量 sizeof(item) == 4
     * =========================================== */
    memset((void *)cmd_head, 0, sizeof(cmd_list_slot));

    assert(cmd_head->CFL <= 0x10);
    cmd_head->CFL = CFL & 0x1f;
    cmd_head->PRDTL = PRDTL;
    assert(!(base & 0x7f));
    cmd_head->CTBA = base >> 7;
    //cmd_head->flag_rcbr = 0b0100;
}

u16 *build_cmd_tab_item(cmd_tab_item *item, size_t size){
    /* ===========================================
     * bug 调试记录
     * sizeof 最好跟类型名！不要跟变量，不然容易出错
     * 就是因为 sizeof(item)，导致内存清除不完全！调试了一个星期
     * 出错原因是 item 是一个指针变量 sizeof(item) == 4
     * =========================================== */
    memset(item, 0, sizeof(cmd_tab_item));

    /* size 必须是偶数，位数不能超过 22 位 */
    assert(!(size % 2) && size && !(size & ~0x3fffff));

    //u16 *data = malloc(size);
    u16 *data = 0x700000;
    printk("[build_cmd_tab_item] vdata_0x%x\n", data);

    item->dba = get_phy_addr(data);
    /* item->dba 最后一位必须为 0 */
    assert(!(item->dba & 1));
    /* dbc 必须是奇数 */
    item->dbc = size - 1;

    printk("[build_cmd_tab_item] pdata_0x%x\n", get_phy_addr(data));

    return data;
}

slot_num load_ata_cmd(hba_port_t *port, u16 **data, u8 cmd, u64 lba, u16 count){
    slot_num free_slot = 32;

    for (int slot = 0; slot < 32; ++slot){
        if (!((port->reg_base[REG_IDX(HBA_PORT_PxCI)] >> slot) & 0x1)){
            free_slot = slot;
            break;
        }
    }

    /* 没找到空闲的插槽 */
    if (free_slot == 32)
        return 32;
    /////////////////////////////////////////////////
    cmd_list_slot *cmd_head = &port->vPxCLB[free_slot];
    cmd_tab_t *cmd_tab = (u32 *)malloc(sizeof(cmd_tab_t) + sizeof(cmd_tab_item));
    //cmd_tab_t *cmd_tab = 0x600000;
    //memset(cmd_tab, 0, sizeof(cmd_tab_t));

    /* ==================================================
     * bug 调试记录
     * 在构建命令头的时候，CFL 的单位是 DW，也就是 CFL == 1 代表四个字节长度
     * 由于传入的时候没有除以 sizeof(u32) 导致 PxTFD.ERR 置一（任务文件错误）
     * ================================================== */
    build_cmd_head(cmd_head, sizeof(FIS_REG_H2D) / sizeof(u32), 1, (u32)get_phy_addr(cmd_tab));
    bulid_cmd_tab_fis((FIS_REG_H2D *)cmd_tab, cmd, lba, count);
    *data = build_cmd_tab_item(cmd_tab->item, 1024);

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
    limit = 0x100000;
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
    /* 没有 hba 设备 */
    if (!probe_hba())
        return;

    hba_GHC_init();
    hba_devices_init();
    
    List_t *devices = hba->devices;
    int dev_num = 0;
    for (ListNode_t *node = devices->end.next; node != &devices->end; node = node->next, ++dev_num){
        hba_dev_t *dev = (hba_dev_t *)node->owner;

        if (dev->last_status == SUCCESSFUL)
            printk("[hba device %d] sata serial_%s\n", dev_num, dev->sata_serial);

        else
            printk("[hba device %d] error code %s\n", dev_num, error_str[dev->last_status]);
    
        printk("[status reg] PxTFD_%x\n", dev->port->reg_base[REG_IDX(HBA_PORT_PxTFD)]);
        printk("[status reg] PxSERR_%x\n", dev->port->reg_base[REG_IDX(HBA_PORT_PxSERR)]);
        printk("[status reg] PxIS_%x\n", dev->port->reg_base[REG_IDX(HBA_PORT_PxIS)]);
    }
}