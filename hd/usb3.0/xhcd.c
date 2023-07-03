#include <rdix/pci.h>
#include <rdix/xhci.h>
#include <rdix/kernel.h>
#include <common/interrupt.h>
#include <rdix/memory.h>
#include <common/string.h>
#include <common/stdlib.h>

#define XHC_LOG_INFO __LOG("[xhc]")
#define XHC_WARNING_INFO __WARNING("[xhc warning]")

xhc_t* xhc;

/* void usb_test(){
    pci_device_t *usb_info = get_device_info(USB_CC);
    u8 SBRN = usb_info->read(usb_info, 0x60);
    printk("SBNR = %x\n", SBRN);
} */

bool probe_xhc(){
    xhc = (xhc_t *)malloc(sizeof(xhc_t));

    xhc->pci_info = get_device_info(USB_CC);

    if (xhc->pci_info == NULL){
        printk(XHC_WARNING_INFO "no xhc device!\n");
        free(xhc);
        return false;
    }

    xhc->cap_base = NULL;

    printk(XHC_LOG_INFO "bar0_addr %p| bar0_size %p\n",
            xhc->pci_info->BAR[0].base_addr, xhc->pci_info->BAR[0].size);
    
    xhc->cap_base = (u32 *)link_nppage(xhc->pci_info->BAR[0].base_addr, xhc->pci_info->BAR[0].size);

    u8 CAPLENGTH = (u8)xhc->cap_base[0];
    u32 RTSOFF = xhc->cap_base[0x18 / 4] & 0xfff0;
    u32 DBOFF = xhc->cap_base[0x14 / 4] & ~3;

    xhc->op_base = (u32*)((u32)CAPLENGTH + (u32)xhc->cap_base);
    xhc->run_base = (u32*)(RTSOFF + (u32)xhc->cap_base);
    xhc->doorbell = (u32*)(DBOFF + (u32)xhc->cap_base);

    u16 HCIVERSION = (u16)(xhc->cap_base[0] >> 16);
    u32 HCSPARAMS1 = xhc->cap_base[1];
    u32 HCSPARAMS2 = xhc->cap_base[2];
    u16 PAGESIZE = (u16)xhc->op_base[2];

    xhc->max_port = HCSPARAMS1 >> 24;
    xhc->max_interrupter = (HCSPARAMS1 >> 8) & 0x3ff;
    xhc->max_slot = HCSPARAMS1 & 0xff;
    xhc->vision = HCIVERSION;
    xhc->max_scratchpad_buf = ((HCSPARAMS2 >> 16) & 0x3e0) | ((HCSPARAMS2 >> 27) & 0x1f);
    xhc->pagesize = 0;

    while (PAGESIZE){
        xhc->pagesize += 1;
        PAGESIZE >>= 1;
    }

    assert(xhc->max_port <= xhc->max_slot);

    return true;
}

void xhc_info(){
    u8 CAPLENGTH = (u8)xhc->cap_base[0];

    printk(XHC_LOG_INFO "length_%x| version_%x| maxPorts_%d| maxInterrupters_%d| maxSlots_%d\n| max_spbuf_%d\n",
            CAPLENGTH,
            xhc->vision,
            xhc->max_port,
            xhc->max_interrupter,
            xhc->max_slot,
            xhc->max_scratchpad_buf);

    printk(XHC_LOG_INFO "PAGESIZE_%x\n", xhc->pagesize);

    while(true);
    
    printk(XHC_LOG_INFO "USBSTS_%x\n", xhc->op_base[1]);
}

static void xhc_handler(u32 int_num);
void xhc_interrupt_init(){
    /* 配置 event ring */
    /* 初始化 Event Ring Segments */
    void *ERS_base = alloc_kpage(INIT_ERST_SIZE);
    memset(ERS_base, 0, INIT_ERS_SIZE);

    /* 初始化 event ring segment table, table 必须64字节对齐 */
    ERST_entry *ERST_base = malloc(sizeof(ERST_entry) * ((INIT_ERST_SIZE + 3) / 4) * 4);
    memset(ERST_base, 0, sizeof(ERST_entry) * ((INIT_ERST_SIZE + 3) / 4) * 4);
    for (int i = 0; i < INIT_ERST_SIZE; ++i){
        assert(((u32)ERS_base & 0x3f) == 0);
        ERST_base[i].base_addr_Lo = (u32)ERS_base & ~0x3f;
        /* 不支持 64 位 */
        ERST_base[i].base_addr_Hi = 0;
        ERST_base[i].size = (INIT_ERS_SIZE / TRB_SIZE) & 0xffff;
    }

    /* 写 interrupt set 0 中 ERST size 和 ERDP 寄存器 */
    int_reg_set *int_set = (int_reg_set *)((u32)xhc->run_base + 0x20);
    //printk("ERDP_Lob_%x\n", int_set[0].ERDP_Lo);
    int_set[0].ERSTSZ = INIT_ERST_SIZE & 0xffff;
    int_set[0].ERDP_Lo = ERST_base[0].base_addr_Lo;
    int_set[0].ERDP_Hi = 0;
    xhc->event_ring.trb = ERST_base[0].base_addr_Lo;
    xhc->event_ring.CCS = 1;
    //printk("ERDP_Lo_%x\n", int_set[0].ERDP_Lo);

    /* 写 ERST base address 寄存器 */
    int_set[0].ERSTBA_Lo = (u32)ERST_base & ~0x3f;
    int_set[0].ERSTBA_Hi = 0;
    //while(true);
    /* 配置 MSI-X */
    u8 vec_arr[1] = {MSI_INT_START + 1};
    size_t size = 1;
    /* 启动 MSI */
    assert(install_MSI_int(xhc->pci_info, MSI_INT_START + XHC_INT_NUM, xhc_handler) == 0);

    /* 配置中断间隔，IMOD: IMODI */
    int_set[0].IMOD = 0;

    /* 启用总中断 */
    xhc->op_base[USBCMD] |= 0b0100;

    /* 启用中断器中断 */
    int_set[0].IMAN |= 0b0011;
}

static void port_status_change_event_proc(){
    printk(XHC_LOG_INFO "in port_status_change_event_proc\n");
    u32 *event_trb = xhc->event_ring.trb;
    u8 port_id = event_trb[0] >> 24;
    u32 *port_reg_set = (u32 *)&xhc->op_base[0x100 + 4 * (port_id - 1)];

    printk(XHC_LOG_INFO "port id = %d\n", port_id);

    /* 如果是 usb2.0，需要额外配置端口寄存器 */
    if (((port_reg_set[0] >> 5) & 0xf) == Polling){
        printk(XHC_LOG_INFO "in usb2 init\n");
        /* 写 PR 位 */
        /* 重置完成后等待进入 enable 状态和新的 psce */
        port_reg_set[0] |= 0x10;
        /* 返回等待下一个 port_status_change_event */
        return;
    }

    /* get slot id */
    /* u32 *cmd_trb = xhc->cmd_ring.trb;
    bool cmd_PCS = xhc->cmd_ring.PCS;
    memset(cmd_trb, 0, TRB_SIZE);
    cmd_trb[3] = (GetEnableSlot << 10) | cmd_PCS;

    cmd_trb += 4;
    xhc->cmd_ring.trb = cmd_trb;
    // todo: 反转 PCS

    u32 n = 0xffff;
    while (n--);
    xhc->doorbell[0] &= ~0xff0f;
    printk("2\n"); */
}

static void cmd_completion_event_proc(){
    printk(XHC_LOG_INFO "in cmd_completion_event_proc\n");
}

static void xhc_handler(u32 int_num){
    printk(XHC_WARNING_INFO "xhc interrupt\n");

    /* 获取首个 event TRB 指针 */
    int_reg_set *int_set = (int_reg_set *)((u32)xhc->run_base + 0x20);
    //printk("int_set->ERDP_Lob = %x\n", int_set->ERDP_Lo);
    u32 *event_trb = xhc->event_ring.trb;
    bool CCS = xhc->event_ring.CCS;

    /* 通过 ccs 来确定是否到达 event ring 末尾 */
    while ((event_trb[3] & 1) == CCS){
        switch ((event_trb[3] >> 10) & 0x3f){
            case PortStatusChangeEvent: port_status_change_event_proc(); break;
            case CmdCompletion: cmd_completion_event_proc(); break;
            default: printk(XHC_LOG_INFO "default trb type.\n"); return;
        }

        event_trb[3] = CCS ? event_trb[3] & ~1 : event_trb[3] | 1;
        event_trb += 4;
    }

    xhc->event_ring.trb = event_trb;
    xhc->event_ring.CCS = CCS;

    int_set->ERDP_Lo = (u32)event_trb | 8;
    int_set->ERDP_Hi = 0;

    printk("event_trb = %x\n", event_trb);

    lapic_send_eoi();
}

void xhc_init(){
    if (!probe_xhc())
        return;
    xhc_info();

    u32 cmd = pci_dev_reg_read(xhc->pci_info, PCI_CONFIG_SPACE_CMD);
    //printk("cmd_%x\n", cmd);

    /* bus master 位的开启非常重要，不开启就无法通过 MSI 产生中断 */
    cmd |= __PCI_CS_CMD_MMIO_ENABLE | __PCI_CS_CMD_BUS_MASTER_ENABLE;

    pci_dev_reg_write(xhc->pci_info, PCI_CONFIG_SPACE_CMD, cmd);

    /* 重置 xhci */
    /* 停止运行 xhci */
    xhc->op_base[USBCMD] &= ~1;
    while (xhc->op_base[USBCMD] & 1);

    /* 重置 xhc */
    xhc->op_base[USBCMD] |= 2;

    /* 等待设别准备完毕 */
    while (((xhc->op_base[USBSTS] >> 11) & 1) || (xhc->op_base[USBCMD] & 2));

    /* 开启的设备插槽数量 */
    u8 maxSlots = 32;
    xhc->op_base[CONFIG] = (u32)maxSlots;
    //printk("xhc->op_base[CONFIG] = %d\n", xhc->op_base[CONFIG]);

    /* 初始化设备上下文地址矩阵指针寄存器 */
    void *DCBAA = malloc(sizeof(u64) * ((maxSlots / 8) + 1) * 8);
    memset(DCBAA, 0, sizeof(u64) * ((maxSlots / 8) + 1) * 8);
    //printk("DCBAA_%x\n", DCBAA);
    assert(((u32)DCBAA & 0x3f) == 0);   // 必须 64 字节对齐
    xhc->op_base[DCBAAP] = (u32)DCBAA;
    xhc->op_base[DCBAAP + 1] = 0;

    /* 初始化 Scratchpad Buffers，不初始化会导致死机 */


    printk("DCBAA_%x\n", xhc->op_base[DCBAAP]);

    /* 初始化 command ring */
    void *cmd_ring_base = alloc_kpage(1);
    memset(cmd_ring_base, 0, PAGE_SIZE);
    /* RCS 位置一 */
    xhc->op_base[CRCR] = (u32)cmd_ring_base;
    xhc->op_base[CRCR + 1] = 0;
    xhc->cmd_ring.trb = cmd_ring_base;
    xhc->cmd_ring.PCS = 0;

    xhc_interrupt_init();

    /* 启动 xhc */
    xhc->op_base[USBCMD] |= 1;

    printk("HCH_%d\n", xhc->op_base[USBSTS] & 1);

    /* u8 vec_arr[1] = {MSI_INT_START + 1};
    size_t size = 1;

    __device_MSI_X_init(xhc->pci_info, 0, xhc->cap_base, vec_arr, size); */

    u32 *cmd_trb = xhc->cmd_ring.trb;
    bool cmd_PCS = xhc->cmd_ring.PCS;
    memset(cmd_trb, 0, TRB_SIZE);
    cmd_trb[3] = (u32)(((9 & 0x3f) << 10));

    cmd_trb += 4;
    cmd_trb[3] = 1;
    xhc->cmd_ring.trb = cmd_trb;
    // todo: 反转 PCS

    xhc->doorbell[0] = 0;
    printk("15\n");
}