#include <rdix/pci.h>
#include <rdix/xhci.h>
#include <rdix/kernel.h>
#include <common/interrupt.h>
#include <rdix/memory.h>
#include <common/string.h>
#include <common/stdlib.h>
#include <rdix/task.h>

#define XHC_LOG_INFO __LOG("[xhc]")
#define XHC_WARNING_INFO __WARNING("[xhc warning]")

xhc_t* xhc;
List_t *dev_enum_list;
ListNode_t *dev_enum_task = NULL;

static general_usb_dev_t usb_device_pool[USBDEVCNT];

/* 获取空闲设备 */
static general_usb_dev_t *get_free_usb_device(){
    // todo: 竞态保护
    for (int i = 0; i < USBDEVCNT; ++i){
        if (usb_device_pool[i].root_port == 0)
            return &usb_device_pool[i];
    }

    return NULL;
}

static general_usb_dev_t *find_usb_device(u32 slot_id){
    assert(slot_id);
    for (int i = 0; i < USBDEVCNT; ++i){
        if (usb_device_pool[i].root_port != 0 && usb_device_pool[i].slot_id == slot_id)
            return &usb_device_pool[i];
    }

    return NULL;
}

bool probe_xhc(){
    xhc = (xhc_t *)malloc(sizeof(xhc_t));

    xhc->pci_info = get_device_info(USB_CC);

    if (xhc->pci_info == NULL){
        printk(XHC_WARNING_INFO "no xhc device!\n");
        free(xhc);
        xhc = NULL;
        return false;
    }

    xhc->cap_base = NULL;

    printk(XHC_LOG_INFO "bar0_addr %p| bar0_size %p\n",
            xhc->pci_info->BAR[0].base_addr, xhc->pci_info->BAR[0].size);
    
    printk(XHC_LOG_INFO "bar1_addr %p\n", xhc->pci_info->BAR[1].base_addr);
    assert(xhc->pci_info->BAR[1].base_addr == 0);
    //有些是64位地址？
    xhc->cap_base = (u32 *)link_nppage(xhc->pci_info->BAR[0].base_addr, xhc->pci_info->BAR[0].size);

    u8 CAPLENGTH = (u8)xhc->cap_base[0];
    u32 RTSOFF = xhc->cap_base[0x18 / 4] & ~0x1f;
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
    xhc->max_scratchpad_buf = HCSPARAMS2 >> 27;
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
    
    printk(XHC_LOG_INFO "USBSTS_%x\n", xhc->op_base[1]);

    //while(true);
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
    int_set[0].ERSTSZ = INIT_ERST_SIZE & 0xffff;
    int_set[0].ERDP_Lo = ERST_base[0].base_addr_Lo;
    int_set[0].ERDP_Hi = 0;
    xhc->event_ring.trb = ERST_base[0].base_addr_Lo;
    xhc->event_ring.cycle_flag = 1;

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

/* 负责初始化一个设备容器，进入枚举任务 */
static void port_status_change_event_proc(u32 *event_trb){
    u8 port_id = event_trb[0] >> 24;
    u32 *port_reg_set = (u32 *)&xhc->op_base[0x100 + 4 * (port_id - 1)];

    //printk(XHC_LOG_INFO "port id = %d\n", port_id);

    /* 如果是 usb2.0，需要额外配置端口寄存器 */
    if (((port_reg_set[0] >> 5) & 0xf) == Polling){
        //printk(XHC_LOG_INFO "in usb2 init\n");
        /* 写 PR 位 */
        /* 重置完成后等待进入 enable 状态和新的 psce */
        port_reg_set[0] |= 0x10;
        /* 返回等待下一个 port_status_change_event */
        return;
    }

    printk(XHC_LOG_INFO "device detect 2\n");

    /* 确保端口已经进入 enable 状态 */
    assert(((port_reg_set[0] >> 5) & 0xf) == U0);

    /* 分配一个空闲设备 */
    // todo: 目前只处理 root hub 的连接，以后完成 hub 驱动后这里需要更改
    general_usb_dev_t *usb_dev = get_free_usb_device();

    if (usb_dev == NULL)
        PANIC("usb device pool is full\n");

    usb_dev->root_port = port_id;
    usb_dev->route_string = 0;
    node_init(&usb_dev->node, usb_dev, 0);
    usb_dev->ctrl_dev = xhc;

    /* 启动枚举线程 */
    list_push(dev_enum_list, &usb_dev->node); //将设备加入枚举链表
    assert(dev_enum_task != NULL);
    if (((TCB_t*)dev_enum_task->owner)->state == TASK_WAITING)
        unblock(dev_enum_task);
}

static void trb_error_proc(u32 *event_trb){
    printk(XHC_WARNING_INFO "in trb_error_proc\n");
    printk("%x| %x| %x| %x\n", event_trb[0], event_trb[1], event_trb[2], event_trb[3]);
    u32 *trb = event_trb[0];
    printk("%x| %x| %x| %x\n", trb[0], trb[1], trb[2], trb[3]);
    printk("target TRB type = %d\n", (trb[3] >> 10) & 0x3f);

    //while(true);
}

static u32 count = 0;
static void xhc_handler(u32 int_num){
    //printk(XHC_WARNING_INFO "xhc interrupt, count %d\n", ++count);
    /* 获取首个 event TRB 指针 */
    int_reg_set *int_set = (int_reg_set *)((u32)xhc->run_base + 0x20);
    //printk("int_set->ERDP_Lob = %x\n", int_set->ERDP_Lo);
    u32 *event_trb = xhc->event_ring.trb;
    bool CCS = xhc->event_ring.cycle_flag;
    u32 event_type = (event_trb[3] >> 10) & 0x3f;

    /* 通过 ccs 来确定是否到达 event ring 末尾 */
    while ((event_trb[3] & 1) == CCS){
        //printk("a\n");
        /* 检查是否是成功执行 */
        if ((event_trb[2] >> 24) == _SUCCESS){
            if (event_type == PortStatusChangeEvent)
                /* 第一次发现设备 */
                port_status_change_event_proc(event_trb);
            else{
                /* command event 和 transfer event 统一处理 */
                u32 *transfer_trb = event_trb[0];
                u32 transfer_type = (transfer_trb[3] >> 10) & 0x3f;
                u32 slot_id = event_trb[3] >> 24;
                general_usb_dev_t *dev = find_usb_device(slot_id);

                /* 如果完成的命令是 enable slot 就需要通过root port等查找设备 */
                if (transfer_type == GetEnableSlot){
                    for (int i = 0; i < USBDEVCNT; ++i){
                        general_usb_dev_t *tmp = &usb_device_pool[i];
                        if (tmp->root_port != 0 && tmp->slot_id == 0 && tmp->wait_task){
                            dev = &usb_device_pool[i];
                            break;
                        }  
                    }
                    if (dev == NULL)
                        PANIC("no usb device need to initialize slot\n");
                }

                if (transfer_type == Normal){
                    printk(XHC_WARNING_INFO "in normal\n");
                    printk("data in int %x\n", transfer_trb[0]);
                    mdebug(transfer_trb[0], 8);
                    //dev->ctrl_dev->doorbell[dev->slot_id] = 3;
                    dev->last_event_trb = event_trb;
                    goto debug;
                }

                assert(dev != NULL);
                dev->last_event_trb = event_trb;
                assert(dev->wait_task);
                assert(dev->wait_trb_type == transfer_type);
                assert (((TCB_t*)dev->wait_task->owner)->state == TASK_BLOCKED);
                unblock(dev->wait_task);
            }
        }
        else
            trb_error_proc(event_trb);
        
debug:
        // todo: next event trb 不能简单地 +4，需要更复杂地操作
        /* CCS 只能用来作为比较。consumer 是只读的，没有写 event trb 的权利 */
        event_trb += 4;
        if (((u32 *)int_set[0].ERSTBA_Lo)[0] / PAGE_SIZE != (u32)event_trb / PAGE_SIZE)
            event_trb = ((u32 *)int_set[0].ERSTBA_Lo)[0];
    }

    xhc->event_ring.trb = event_trb;
    xhc->event_ring.cycle_flag = CCS;

    int_set->ERDP_Lo = (u32)event_trb | 8;
    int_set->ERDP_Hi = 0;

    lapic_send_eoi();
}

void xhc_init(){
    if (!probe_xhc())
        return;
    xhc_info();
    
    dev_enum_list = new_list();
    
    /* 初始化设备池 */
    memset(usb_device_pool, 0, sizeof(general_usb_dev_t) * USBDEVCNT);
    printk("usb_device_pool size %d\n",sizeof(usb_device_pool));
    //while(true);
    /* 配置 pci 配置空间 command control 寄存器 */
    u32 cmd = pci_dev_reg_read(xhc->pci_info, PCI_CONFIG_SPACE_CMD);
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
    u8 maxSlots = xhc->max_slot;
    xhc->op_base[CONFIG] = (u32)maxSlots;

    /* 初始化设备上下文地址矩阵指针寄存器 */
    void *DCBAA = malloc(sizeof(u64) * ((maxSlots / 8) + 1) * 8);
    memset(DCBAA, 0, sizeof(u64) * ((maxSlots / 8) + 1) * 8);
    assert(((u32)DCBAA & 0x3f) == 0);   // 必须 64 字节对齐
    xhc->op_base[DCBAAP] = (u32)DCBAA;
    xhc->op_base[DCBAAP + 1] = 0;

    /* 初始化 Scratchpad Buffers，不初始化会导致死机 */
    u64 *sb_array = malloc(sizeof(u64) * xhc->max_scratchpad_buf);
    assert(((u32)DCBAA & 0x3f) == 0);   // 必须 64 字节对齐
    *(u64 *)DCBAA = (u64)sb_array;

    /* 初始化 Scratchpad Buffers array 中的表项*/
    printk("xhc->max_scratchpad_buf = %d\n", xhc->max_scratchpad_buf);
    printk("pagesize_%d\n", xhc->pagesize);
    for (int i = 0; i < xhc->max_scratchpad_buf; ++i){
        printk("i = %d\n", i);
        sb_array[i] = (u64)alloc_kpage(xhc->pagesize);
        if (sb_array[i] == NULL){
            PANIC("out of memory\n");
        }

        //memset((u32)sb_array[i], 0, PAGE_SIZE * xhc->pagesize);
    }

    //printk("DCBAA_%x\n", xhc->op_base[DCBAAP]);

    /* 初始化 command ring */
    void *cmd_ring_base = alloc_kpage(1);
    if (cmd_ring_base == NULL){
        PANIC("out of memory\n");
    }
    
    memset(cmd_ring_base, 0, PAGE_SIZE);

    /* 在末尾初始化 link TRB */
    u32 *tail_trb = (u32 *)((u32)cmd_ring_base + PAGE_SIZE - TRB_SIZE);
    tail_trb[0] = (u32)cmd_ring_base;
    tail_trb[3] = (LinkTRB << 10) | 2 | 1;
    /* PCS CCS 初始化为 1 */
    xhc->op_base[CRCR] = (u32)cmd_ring_base | 1;
    xhc->op_base[CRCR + 1] = 0;
    xhc->cmd_ring.trb = cmd_ring_base;
    xhc->cmd_ring.cycle_flag = 1;

    xhc_interrupt_init();

    /* 启动 xhc */
    xhc->op_base[USBCMD] |= 1;

    //printk("HCH_%d\n", xhc->op_base[USBSTS] & 1);

    /* u8 vec_arr[1] = {MSI_INT_START + 1};
    size_t size = 1;

    __device_MSI_X_init(xhc->pci_info, 0, xhc->cap_base, vec_arr, size); */
    //while(true);
}