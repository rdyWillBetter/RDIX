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

#define PCI_LOG_INFO(fmt, args...) logk("PCI INFO", fmt, ##args)

typedef struct PCI_tree_t{
    int32 bus_num;
    /* 当前总线的设备链表 */
    List_t *dev_list;
    /* 该总线的子总线 */
    struct PCI_tree_t *child;
} PCI_tree_t;

typedef struct device_t{
    u16 vectorID;
    u16 deviceID;
    u16 command;
    u16 status;
    u8 revisionID;
    u32 Ccode : 24;
    u8 cacheline_size;
    u8 latency_timer;
    u8 header_type;
    u8 BIST;
    u32 BAR[6];
    u32 CCpointer;
    u16 sub_vector_ID;
    u16 sub_ID;
    u32 ExROMbaddr;
    u8 Capability_pointer;
    u32 reserved1 : 24;
    u32 reserved2;
    u8 interrupt_line;
    u8 interrupt_pin;
    u8 min_gnt;
    u8 max_lat;
} _packed device_t;

void PCI_init();
void PCI_info();

#endif