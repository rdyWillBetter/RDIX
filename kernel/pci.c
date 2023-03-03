#include <rdix/pci.h>
#include <common/io.h>
#include <rdix/kernel.h>


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

static u32 read_register(u8 bus, u8 dev_num, u8 function, u8 reg){
    /* 访问非直接接在 PCI 主桥上的设备时( bus != 0 )，需要使用 Type 1 格式的寻址指令访问 */
    u8 type = bus > 0 ? 1 : 0;
    u32 addr = GET_TRANSCATION(bus, dev_num, function, reg, type);

    port_outd(PCI_CONFIG_ADDRESS, addr);
    return port_ind(PCI_CONFIG_DATA);
}

static device_t *device_probe(List_t **list, u8 bus, u8 dev_num, u8 function){
    u32 data = read_register(bus, dev_num, function, 0);

    if (!DEVICE_EXIST((u16)data))
        return NULL;
    
    /* 传入的设备链表头指针可能是 NULL，如果这条总线上有设备的话需要生成一个头节点 */
    if (!(*list))
        *list = new_list();

    u32 *Cspace = (u32 *)malloc(256);

    Cspace[0] = data;

    for (int i = 1; i < 256 / sizeof(u32); ++i){
        Cspace[i] = read_register(bus, dev_num, function, i * sizeof(u32));
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
            int secondary_bus_num = (u8)(device->BAR[2] >> 8);
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
    device_t *device = NULL;

    for (PCI_tree_t *tree_ptr = device_list; tree_ptr != NULL && tree_ptr->bus_num != -1; tree_ptr = tree_ptr->child){

        for (ListNode_t *node = tree_ptr->dev_list->end.next; node != &tree_ptr->dev_list->end; node = node->next){

            device = (device_t *)node->owner;
            PCI_LOG_INFO("Bus number = %d, Vector ID = 0x%x, Device ID = 0x%x\n",\
                        tree_ptr->bus_num, device->vectorID, device->deviceID);
        }
    }

    
}