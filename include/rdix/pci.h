#ifndef __PCI_H__
#define __PCI_H__

#include <common/type.h>
#include <rdix/kernel.h>
#include <common/list.h>

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc

/* header type 类型指明 64 bit 的后续布局 */
/* 指明该配置空间是 PCI 桥布局 */
#define PCI_BRIDGE 0x1

/* 配置空间寄存器地址 */
#define PCI_CONFIG_SPACE_CMD 0x4
#define PCI_CONFIG_SPACE_CAP_PTR 0x34

/* 能力链表中的能力id */
#define PCI_CAP_ID_MSI 0x5

/* 能力链表尾指针 */
#define PCI_CAP_END_PTR 0

enum PCI_CS_CMD_FLAGS{
    __PCI_CS_CMD_MMIO_ENABLE = (1 << 1),
    __PCI_CS_CMD_BUS_MASTER_ENABLE = (1 << 2),
    __PCI_CS_CMD_BUS_INT_DISABLE = (1 << 10),
};

typedef struct PCI_tree_t{
    int32 bus_num;
    /* 当前总线的设备链表 */
    List_t *dev_list;
    /* 该总线的子总线 */
    struct PCI_tree_t *child;
} PCI_tree_t;

typedef struct bar_entry{
    u32 base_addr;
    u32 size;
    u8 type;
} bar_entry;

typedef struct device_t{
    /* 该设备配置空间首地址 */
    u32 bus;
    u8 dev_num;
    u8 function;
    u16 vectorID;
    u16 deviceID;
    u8 revisionID;
    u32 Ccode;
    u8 header_type;
    bar_entry BAR[6];
} pci_device_t;

void PCI_init();
void PCI_info();

u32 read_register(u8 bus, u8 dev_num, u8 function, u8 reg);
void write_register(u8 bus, u8 dev_num, u8 function, u8 reg, u32 data);

pci_device_t *get_device_info(u32 dev_cc);

typedef u8 cap_p_t;

cap_p_t capability_search(pci_device_t *dev, u8 cap_id);

int __device_MSI_init(pci_device_t *dev, u8 vector);

#endif