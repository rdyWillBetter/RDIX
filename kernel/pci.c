#include <rdix/pci.h>
#include <common/io.h>
#include <rdix/kernel.h>
#include <common/assert.h>

#define PCI_LOG_INFO __LOG("[pci info]")
#define PCI_WARNING_INFO __WARNING("[pci warning]")

#define GET_TRANSCATION(bus, device, function, register, type) \
(0x80000000 | (bus << 16) | ((device & 0x1f) << 11) | ((function & 7) << 8) | (register & 0xFC) | (type & 3))

#define DEVICE_EXIST(vectorID) (vectorID != 0xffff)

PCI_tree_t *device_list;

static PCI_tree_t *new_pci_tree(){
    PCI_tree_t *master = (PCI_tree_t *)malloc(sizeof(PCI_tree_t));

    master->child = NULL;
    master->dev_list = NULL;
    master->bus_num = -1;

    return master;
}

u32 read_register(u8 bus, u8 dev_num, u8 function, u8 reg){
    /* 访问非直接接在 PCI 主桥上的设备时( bus != 0 )，需要使用 Type 1 格式的寻址指令访问 */
    u8 type = bus > 0 ? 1 : 0;
    u32 addr = GET_TRANSCATION(bus, dev_num, function, reg, type);

    port_outd(PCI_CONFIG_ADDRESS, addr);
    return port_ind(PCI_CONFIG_DATA);
}

void write_register(u8 bus, u8 dev_num, u8 function, u8 reg, u32 data){
    u8 type = bus > 0 ? 1 : 0;
    u32 addr = GET_TRANSCATION(bus, dev_num, function, reg, type);

    port_outd(PCI_CONFIG_ADDRESS, addr);
    port_outd(PCI_CONFIG_DATA, data);
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

static device_t *device_probe(List_t **list, u8 bus, u8 dev_num, u8 function){
    u32 data = read_register(bus, dev_num, function, 0);

    if (!DEVICE_EXIST((u16)data))
        return NULL;
    
    /* 传入的设备链表头指针可能是 NULL，如果这条总线上有设备的话需要生成一个头节点 */
    if (!(*list))
        *list = new_list();

    device_t *Cspace = (device_t *)malloc(sizeof(device_t));

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

    return (device_t *)Cspace;
}

/* 获取总线号 bus 上的全部设备 */
static void get_all_device(u8 bus, PCI_tree_t *tree){
    
    tree->bus_num = bus;

    for (size_t dev_num = 0; dev_num < 32; ++dev_num){

        /* 探测一个设备，如果存在，就将其并入设备链表，否则什么都不做，并返回 NULL */
        device_t *device = device_probe(&tree->dev_list, bus, dev_num, 0);

        if (!device)
            continue;
        
        /* 桥设备 */
        if (device->header_type == 1){
            /* 取出下一个总线号 */
            int secondary_bus_num = (u8)(device->BAR[2].base_addr >> 8);
            tree->child = new_pci_tree();
            get_all_device(secondary_bus_num, tree->child);
        }

        /* 多功能检测 */
        if (device->header_type & 0x80)
            for (int i = 1; i < 8; ++i){
                device_probe(&tree->dev_list, bus, dev_num, i);
            }
    }
}

void PCI_init(){
    device_list = new_pci_tree();

    get_all_device(0, device_list);
}

void PCI_info(){

    for (PCI_tree_t *tree_ptr = device_list; tree_ptr != NULL && tree_ptr->bus_num != -1; tree_ptr = tree_ptr->child){

        for (ListNode_t *node = tree_ptr->dev_list->end.next; node != &tree_ptr->dev_list->end; node = node->next){

            device_t *device = (device_t *)node->owner;

            printk(PCI_LOG_INFO "Bus_%d; vid_%x; dev_%x; CC_%x; bar6_%x; size_%x\n",\
                        tree_ptr->bus_num, device->vectorID, device->deviceID, device->Ccode,\
                        device->BAR[5].base_addr, device->BAR[5].size);

            if (device->Ccode == 0x010601){
                void *cap_list = read_register(device->bus, device->dev_num, device->function, 0x34);
                printk(PCI_WARNING_INFO "hba Capabilities Pointer 0x%p\n", cap_list);
            }
        }
    }
}

device_t *get_device_info(u32 dev_cc){
    device_t *device = NULL;
    
    for (PCI_tree_t *tree_ptr = device_list; tree_ptr != NULL && tree_ptr->bus_num != -1; tree_ptr = tree_ptr->child){

        for (ListNode_t *node = tree_ptr->dev_list->end.next; node != &tree_ptr->dev_list->end; node = node->next){

            device = (device_t *)node->owner;

            if(device->Ccode == dev_cc){
                return device;
            }
            
        }
    }

    return NULL;
}

/* 返回能力链表指针 */
cap_p_t capability_search(device_t *dev, u8 cap_id){
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
int __device_MSI_INIT(device_t *dev, u8 vector){
    u32 cmd = read_register(dev->bus, dev->dev_num,\
                            dev->function, PCI_CONFIG_SPACE_CMD);
    
    /* 要使用 MSI，首先要屏蔽传统中断引脚 */
    cmd |= __PCI_CS_CMD_BUS_INT_DISABLE;

    write_register(dev->bus, dev->dev_num,\
                dev->function, PCI_CONFIG_SPACE_CMD, cmd);
    
    cap_p_t cap_p = capability_search(dev, PCI_CAP_ID_MSI);

    if (cap_p == PCI_CAP_END_PTR)
        return -1;

    u32 reg = read_register(dev->bus,\
                            dev->dev_num,
                            dev->function,
                            cap_p);

    assert((reg & 0xff) == PCI_CAP_ID_MSI);
    /* 启用 MSI */
    reg |= (1 << 16);

    write_register(dev->bus,\
                dev->dev_num,
                dev->function,
                cap_p, reg);
    
    printk(PCI_LOG_INFO "reg %x\n", reg);

    u8 msg_data_p = cap_p;
    u8 msg_addr_p = cap_p + 0x4;
    /* 检测是否支持 64 位 */
    if (reg & (1 << 23)){
        /* 支持 64位 */
        msg_data_p += 0xc;

        write_register(dev->bus,\
                    dev->dev_num,
                    dev->function,
                    0x8, 0);
    }
    else{
        msg_data_p += 0x8;
    }
    
    u32 msg_data = read_register(dev->bus,\
                            dev->dev_num,
                            dev->function,
                            msg_data_p);
    
    printk(PCI_LOG_INFO "msg_data %x\n", msg_data);

    /* edge, fixed */
    msg_data = vector;

    u32 msg_addr = 0xfee00000;

    write_register(dev->bus,\
                dev->dev_num,
                dev->function,
                msg_data_p, msg_data);

    write_register(dev->bus,\
                dev->dev_num,
                dev->function,
                msg_addr_p, msg_addr);

    return 0;
}