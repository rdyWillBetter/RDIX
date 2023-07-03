#include <rdix/pci.h>
#include <common/io.h>
#include <rdix/kernel.h>
#include <common/assert.h>

#define PCI_LOG_INFO __LOG("[pci info]")
#define PCI_WARNING_INFO __WARNING("[pci warning]")

#define GET_TRANSCATION(bus, device, function, register, type) \
(0x80000000 | (bus << 16) | ((device & 0x1f) << 11) | ((function & 7) << 8) | (register & 0xFC) | (type & 3))

#define DEVICE_EXIST(vectorID) (vectorID != 0xffff)

PCI_bus_t *bus_list;

static PCI_bus_t *new_pci_bus(){
    PCI_bus_t *master = (PCI_bus_t *)malloc(sizeof(PCI_bus_t));

    master->next = NULL;
    master->dev_list = NULL;
    master->bus_num = -1;

    return master;
}

static u32 read_register(u32 bus, u8 dev_num, u8 function, u8 reg){
    /* 访问非直接接在 PCI 主桥上的设备时( bus != 0 )，需要使用 Type 1 格式的寻址指令访问 */
    u8 type = bus > 0 ? 1 : 0;
    u32 addr = GET_TRANSCATION(bus, dev_num, function, reg, type);

    port_outd(PCI_CONFIG_ADDRESS, addr);
    return port_ind(PCI_CONFIG_DATA);
}

static void write_register(u32 bus, u8 dev_num, u8 function, u8 reg, u32 data){
    u8 type = bus > 0 ? 1 : 0;
    u32 addr = GET_TRANSCATION(bus, dev_num, function, reg, type);

    port_outd(PCI_CONFIG_ADDRESS, addr);
    port_outd(PCI_CONFIG_DATA, data);
}

u32 pci_dev_reg_read(pci_device_t *dev, u8 reg){
    return read_register(dev->bus, dev->dev_num, dev->function, reg);
}

void pci_dev_reg_write(pci_device_t *dev, u8 reg, u32 data){
    write_register(dev->bus, dev->dev_num, dev->function, reg, data);
}

static void get_bar_and_size(bar_entry *bar, u32 bus, u8 dev_num, u8 function, u8 bar_idx){
    u8 reg = 0x10 + bar_idx * 4;
    u32 addr = GET_TRANSCATION(bus, dev_num, function, reg, bus > 0 ? 1 : 0);

    port_outd(PCI_CONFIG_ADDRESS, addr);

    bar->base_addr = port_ind(PCI_CONFIG_DATA);

    port_outd(PCI_CONFIG_DATA, 0xffffffff);

    bar->size = port_ind(PCI_CONFIG_DATA);

    port_outd(PCI_CONFIG_DATA, bar->base_addr);

        /* bar 中第 0 位是 0 代表指向的是内存空间，配置项为后四位。
     * 为 1 代表指向的是 IO 空间，配置项为后两位。 */
    if (bar->base_addr & 1){
        /* IO */
        bar->size &= 0xfffffffc;
        bar->type = bar->base_addr & 0x3;
        bar->base_addr &= 0xfffffffc;
    }
    else {
        /* memory */
        bar->size &= 0xfffffff0;
        bar->type = bar->base_addr & 0xf;
        bar->base_addr &= 0xfffffff0;
    }

    bar->size = (~bar->size) + 1;
}

static pci_device_t *device_probe(List_t **list, u8 bus, u8 dev_num, u8 function){
    u32 data = read_register(bus, dev_num, function, 0);

    if (!DEVICE_EXIST((u16)data))
        return NULL;
    
    /* 传入的设备链表头指针可能是 NULL，如果这条总线上有设备的话需要生成一个头节点 */
    if (!(*list))
        *list = new_list();

    pci_device_t *Cspace = (pci_device_t *)malloc(sizeof(pci_device_t));

    Cspace->bus = bus;
    Cspace->dev_num = dev_num;
    Cspace->function = function;

    Cspace->vectorID = data & 0xffff;
    Cspace->deviceID = (data >> 16) & 0xffff;

    data = read_register(bus, dev_num, function, 0x8);

    Cspace->revisionID = data & 0xff;
    Cspace->Ccode = (data >> 8) & 0xffffff;

    data = read_register(bus, dev_num, function, 0xc);

    Cspace->header_type = (data >> 16) & 0xff;

    for (int bar = 0; bar < 6; ++bar){
        get_bar_and_size(&Cspace->BAR[bar], bus, dev_num, function, bar);
    }

    ListNode_t *node = new_listnode(Cspace, 0);
    list_pushback(*list, node);

    return (pci_device_t *)Cspace;
}

/* 获取总线号 bus 上的全部设备 */
static void get_all_device(PCI_bus_t *bus){

    for (size_t dev_num = 0; dev_num < 32; ++dev_num){

        /* 探测一个设备，如果存在，就将其并入设备链表，否则什么都不做，并返回 NULL */
        pci_device_t *device = device_probe(&bus->dev_list, bus->bus_num, dev_num, 0);

        if (!device)
            continue;
        
        /* 桥设备 */
        //if (device->header_type == 1){
          /* 取出下一个总线号 */
        //    int secondary_bus_num = (u8)(device->BAR[2].base_addr >> 8);
        //    tree->child = new_pci_tree();
        //    get_all_device(secondary_bus_num, tree->child);
        //}

        /* 多功能检测 */
        if (device->header_type & 0x80)
            for (int i = 1; i < 8; ++i){
                device_probe(&bus->dev_list, bus->bus_num, dev_num, i);
            }
    }
}

void PCI_init(){
    bus_list = new_pci_bus();
    PCI_bus_t *p = bus_list, *pre = p;

    for (int i = 0; i < 256; ++i){
        p->bus_num = i;
        get_all_device(p);

        if (p->dev_list && i < 255){
            p->next = new_pci_bus();
            pre = p;
            p = p->next;
        }
    }

    if (!p->dev_list && pre != p){
        pre->next = NULL;
        free(p);
    }

    PCI_info();
}

void PCI_info(){

    for (PCI_bus_t *bus_ptr = bus_list; bus_ptr != NULL; bus_ptr = bus_ptr->next){

        for (ListNode_t *node = bus_ptr->dev_list->end.next; node != &bus_ptr->dev_list->end; node = node->next){

            pci_device_t *device = (pci_device_t *)node->owner;

            printk(PCI_LOG_INFO "Bus_%d; vid_%x; dev_%x; CC_%x; func_%x; devnum_%x\n",\
                        bus_ptr->bus_num, device->vectorID, device->deviceID, device->Ccode,\
                        device->function, device->dev_num);

            /* if (device->header_type == 0x81){
                void *cap_list = read_register(device->bus, device->dev_num, device->function, 0x34);
                printk(PCI_WARNING_INFO "header_type == 0x81, cc_%x\n", device->Ccode);
            } */
        }
    }
}

pci_device_t *get_device_info(u32 dev_cc){
    pci_device_t *device = NULL;
    
    for (PCI_bus_t *bus_ptr = bus_list; bus_ptr != NULL; bus_ptr = bus_ptr->next){

        for (ListNode_t *node = bus_ptr->dev_list->end.next; node != &bus_ptr->dev_list->end; node = node->next){

            device = (pci_device_t *)node->owner;

            if(device->Ccode == dev_cc){
                return device;
            }
            
        }
    }

    return NULL;
}

/* 返回能力链表指针 */
cap_p_t capability_search(pci_device_t *dev, u8 cap_id){
    u8 capabilities = read_register(dev->bus, dev->dev_num, dev->function, PCI_CONFIG_SPACE_CAP_PTR) & 0xff;

    if (capabilities == PCI_CAP_END_PTR)
        return PCI_CAP_END_PTR;

    cap_p_t cap_ptr = capabilities;
    u32 cap_reg = read_register(dev->bus, dev->dev_num, dev->function, cap_ptr);

    while (true){
        if ((cap_reg & 0xff) == cap_id)
            return cap_ptr;
        
        cap_ptr = (cap_reg >> 8) & 0xff;

        if (cap_ptr == PCI_CAP_END_PTR)
            break;
        
        cap_reg = read_register(dev->bus, dev->dev_num, dev->function, cap_ptr);
    }

    return PCI_CAP_END_PTR;
}

/* 初始化 PCI 设备配置空间能力链表中的 MSI 能力 */
int __device_MSI_init(pci_device_t *dev, u8 vector){
    u32 cmd = pci_dev_reg_read(dev, PCI_CONFIG_SPACE_CMD);
    
    /* 要使用 MSI，首先要屏蔽传统中断引脚 */
    cmd |= __PCI_CS_CMD_BUS_INT_DISABLE;

    pci_dev_reg_write(dev, PCI_CONFIG_SPACE_CMD, cmd);
    
    cap_p_t cap_p = capability_search(dev, PCI_CAP_ID_MSI);

    if (cap_p == PCI_CAP_END_PTR)
        return EOF;

    u32 reg = pci_dev_reg_read(dev, cap_p);

    assert((reg & 0xff) == PCI_CAP_ID_MSI);
    /* 启用 MSI */
    reg |= (1 << 16);
    pci_dev_reg_write(dev, cap_p, reg);
    
    //printk(PCI_LOG_INFO "reg %x\n", reg);

    u8 msg_data_p = cap_p;
    u8 msg_addr_p = cap_p + 0x4;
    u8 mask_bit_map = cap_p + 0xc;
    /* 检测是否支持 64 位 */
    if (reg & (1 << 23)){
        /* 支持 64位 */
        msg_data_p += 0xc;
        mask_bit_map += 0x4;

        pci_dev_reg_write(dev, 0x8, 0);
    }
    else{
        msg_data_p += 0x8;
    }

    /* 支持 mask */
    if ((reg >> 24) & 1)
        pci_dev_reg_write(dev, mask_bit_map, 0);
    
    u32 msg_data = pci_dev_reg_read(dev, msg_data_p);
    
    //printk(PCI_LOG_INFO "msg_data %x\n", msg_data);

    /* edge, fixed */
    msg_data = vector;

    u32 msg_addr = 0xfee00000;

    pci_dev_reg_write(dev, msg_data_p, msg_data);

    pci_dev_reg_write(dev, msg_addr_p, msg_addr);

    printk(PCI_LOG_INFO "MSI init success\n");
    return 0;
}

u64 *debug_pba = 1;

/* 配置 MSI-X capability，used_bar 代表已经映射过的 bar，防止重复映射
 * mapped addr 对应used_bar映射到的虚拟地址
 * vec_array 需要装载的向量数组
 * arr_size 数组大小 */
MSI_X_TABLE_ENTRY *__device_MSI_X_init(pci_device_t *dev, u8 used_bar, void *mapped_addr,
                                        u8 *vec_array, size_t arr_size){
    u32 cmd = pci_dev_reg_read(dev, PCI_CONFIG_SPACE_CMD);
    
    /* 要使用 MSI，首先要屏蔽传统中断引脚 */
    cmd |= __PCI_CS_CMD_BUS_INT_DISABLE;

    pci_dev_reg_write(dev, PCI_CONFIG_SPACE_CMD, cmd);

    cap_p_t cap_p = capability_search(dev, PCI_CAP_ID_MSI_X);

    if (cap_p == PCI_CAP_END_PTR)
        return NULL;

    u32 reg1 = pci_dev_reg_read(dev, cap_p);
    u32 reg2 = pci_dev_reg_read(dev, cap_p + 4);
    u32 reg3 = pci_dev_reg_read(dev, cap_p + 8);

    printk(PCI_LOG_INFO "MSI-X reg1_%x\n", reg1);

    u8 table_bar = reg2 & 7;
    u8 PBA_bar = reg3 & 7;
    MSI_X_TABLE_ENTRY *table_base = NULL;
    u64 *PBA_base = NULL;

    if (table_bar != used_bar)
        table_base = (MSI_X_TABLE_ENTRY *)link_nppage(dev->BAR[table_bar].base_addr,
                                                        dev->BAR[table_bar].size);
    else
        table_base = (MSI_X_TABLE_ENTRY *)mapped_addr;

    /* 偏移 */
    /* 指针不能直接加整型，因为指针指向的数据类型长度会影响相加后的值 */
    table_base = (MSI_X_TABLE_ENTRY *)((u32)table_base + (reg2 & 0xfffffff8));

    if (PBA_bar != used_bar)
        PBA_base = (u64 *)link_nppage(dev->BAR[PBA_bar].base_addr,
                                                        dev->BAR[PBA_bar].size);
    else
        PBA_base = (u64 *)mapped_addr;

    PBA_base = (u64 *)((u32)PBA_base + (reg3 & 0xfffffff8));
    printk(PCI_LOG_INFO "PBA_%x\n", *PBA_base);
    debug_pba = PBA_base;
    printk(PCI_LOG_INFO "debug_pbab_%x\n", debug_pba);

    for (int i = 0; i < arr_size; ++i){
        table_base[i].Msg_Addr = 0xfee00000;
        table_base[i].Msg_Upper_Addr = 0;
        table_base[i].Msg_Data = vec_array[i];
        table_base[i].Vector_Control &= ~1;
    }

    /* enable MSI-X */
    reg1 |= 0x80000000;
    pci_dev_reg_write(dev, cap_p, reg1);

    reg1 = pci_dev_reg_read(dev, cap_p);

    printk(PCI_LOG_INFO "MSI-X reg1b_%x; reg2_%x; reg3_%x\n", reg1, reg2, reg3);

    printk(PCI_LOG_INFO "MSI-X initial success\n");

    return table_base;
}