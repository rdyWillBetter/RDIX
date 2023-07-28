#ifndef __USB_H__
#define __USB_H__

#include <common/type.h>
#include <rdix/pci.h>
#include <common/list.h>
#include <rdix/usbDescriptor.h>

#define PORT_SPEED(ctrl, port_id) ((ctrl->op_base[0x100 + (4 * (port_id - 1))] >> 10) & 0xf)

/* 支持的最大 usb 设备个数 */
#define USBDEVCNT 8

/* xhc class code */
#define USB_CC 0x0c0330

#define TRB_SIZE 0x10

/* xhc 初始化参数 */
#define INIT_ERST_SIZE 1
#define INIT_ERS_SIZE PAGE_SIZE
#define INIT_MAX_ENABLE_SLOT 16

/* xhc 寄存器偏移 */
/* operational 寄存器 */
#define USBCMD 0
#define USBSTS 1
#define CONFIG (0x38 / 4)
#define DCBAAP (0x30 / 4)
#define CRCR (0x18 / 4)

/* PORTSC 相关 */
/* PLS 状态 */
#define U0 0
#define U1 1
#define U2 2
#define U3 3
#define Disabled 4
#define RxDetect 5
#define Inactive 6
#define Polling 7
#define Recovery 8
#define HotReset 9
#define ComplianceMode 10
#define TestMode 11
#define Resume 15

/* TRB type */
#define TransferEvent 32
#define PortStatusChangeEvent 34
#define CmdCompletion 33
#define LinkTRB 6
#define SetupStage 2
#define DataStage 3
#define StatusStage 4
#define Normal 1

/* command TRB专用 */
typedef enum cmd_TRB_type{
    GetEnableSlot = 9,
    AddressDevice = 11,
    ConfigureEndpoint = 12,
} cmd_TRB_type;

enum usb_speed_id{
    Full_speed = 1,
    Low_speed = 2,
    High_speed = 3,
    SuperSpeed = 4,
    SuperSpeedPlus = 5,
};

enum slot_state{
    slot_Disabled,
    slot_Enabled,
    slot_Default,
    slot_Addressed,
    slot_Configured,
};

/* setup stage 中 TRT 位 */
enum transfer_type{
    NoDataStage,
    Rsvd,
    OUTDataStage,
    INDataStage,
};

enum transfer_direction{
    __OUT,
    __IN,
};

/* 控制传输请求类型 */
enum standard_request_code{
    GET_STATUS = 0,
    CLEAR_FEATURE = 1,
    SET_FEATURE = 3,
    SET_ADDRESS = 5,
    GET_DESCRIPORT = 6,
    SET_DESCRIPORT = 7,
    GET_CONFIGURATION = 8,
    SET_CONFIGURATION = 9,
    GET_INTERFACE = 10,
    SET_INTERFACE = 11,
    SYNCH_FRAME = 12,
    SET_SEL = 48,
    SET_ISOCH_DELAY = 49,
};

/* endpoint type */
#define EP_TYPE_CONTROL 4
#define EP_TYPE_INTERRUPT_OUT 3

/* event TRB completion code */
#define _SUCCESS 1

/* context data struct */
#pragma region

struct input_control_contex{
    u32 drop_flag;
    u32 add_flag;
    u32 rsvdz[5];
    u8 config_value;
    u8 interface_number;
    u8 alternate_setting;
    u8 rsvdz1;
} _packed;

struct slot_context{
    u32 route_string : 20;
    u8 speed : 4;
    u8 rsvdz1 : 1;
    u8 MTT : 1;
    u8 hub : 1;
    u8 contex_entries : 5;

    u16 max_exit_latency;
    u8 root_hub_port_number;
    u8 number_of_ports;

    u8 TT_hub_slot_id;
    u8 TT_port_number;
    u8 TTT : 2;
    u8 rsvdz2 : 4;
    u32 interrupt_target : 10;

    u8 usb_device_address;
    u32 rxvdz3 : 17;
    u8 slot_state : 5;

    u32 resvdo[4];
} _packed;

struct endpoint_context{
    u8 EP_state : 3;
    u8 rsvdz1 : 5;
    u8 mult : 2;
    u8 maxPstreams : 5;
    u8 LSA : 1;
    u8 interval;
    u8 max_ESIT_payload_hi;

    u8 rsvdz2 : 1;
    u8 CErr : 2;
    u8 EP_type : 3;
    u8 rsvdz3 : 1;
    u8 HID : 1;
    u8 max_burst_size;
    u16 max_packet_size;
    
    u32 TR_dequeue_point_lo;

    u32 TR_dequeue_point_hi;

    u16 areverage_TRB_length;
    u16 max_ESIT_payload_lo;

    u32 revdo[3];
} _packed;

#define EPContextTotalNum 31

typedef struct device_context_t{
    struct slot_context slot_control;
    struct endpoint_context EP[EPContextTotalNum];
} device_context_t;

typedef struct input_context_t{
    struct input_control_contex input_control;
    device_context_t device_context;
} input_context_t;

#pragma endregion

struct ring_t{
    u32 *trb;
    bool cycle_flag;
};

struct usb_dev_state_t{
    u32 slot_state;
};

typedef struct xhc_t xhc_t;

typedef struct endpoint_m{
    struct ring_t *ring;
    ep_desc_t desc;
    ListNode_t node;
} endpoint_m;

typedef struct general_usb_dev_t{
    u8 root_port;
    u32 route_string;
    u32 slot_id;
    xhc_t *ctrl_dev;

    endpoint_m ep[31];
    //struct ring_t *transfer_ring[31];
    struct usb_dev_state_t state;
    u32 *last_event_trb; // 该设备当前需要处理的事件trb

    ListNode_t node;
    ListNode_t *wait_task;
    u32 wait_trb_type;

    device_descriptor_m desc_tree;
} general_usb_dev_t;

typedef struct xhc_t{
    pci_device_t* pci_info;
    u32 *cap_base;
    u32 *op_base;
    u32 *run_base;
    u32 *doorbell;
    struct ring_t cmd_ring;
    struct ring_t event_ring;

    /* 基本信息 */
    u8 max_port;
    u8 max_slot;
    u16 max_interrupter;
    u16 vision;
    u16 max_scratchpad_buf; // entry 数量
    u8 pagesize;    // scratchpad_buf 对齐页数
} xhc_t;

typedef struct ERST_entry{
    u32 base_addr_Lo; //低六位保留不用
    u32 base_addr_Hi;
    u32 size; //实际有用部分为低2字节
    u32 Rsvd;
} ERST_entry;

typedef struct int_reg_set{
    u32 IMAN;   //interrupt management register
    u32 IMOD;   //interrupt moderation register
    u32 ERSTSZ; //event ring segment table size register
    u32 Rsvd;
    u32 ERSTBA_Lo; //event ring segment table base address register
    u32 ERSTBA_Hi;
    u32 ERDP_Lo;   //event ring dequeue pointer register
    u32 ERDP_Hi;
} int_reg_set;

void usb_device_enumeration();

/* command ring 命令 */
void xhc_cmd_enable_slot(general_usb_dev_t *dev);
void xhc_cmd_address_device(general_usb_dev_t *dev, u32 *input_context_point, bool BSR);

/* transfer ring request */
void xhc_transfer_get_descriptor(general_usb_dev_t *dev, u8 desc_type, u8 desc_index, void *buf, u16 bfsize);
void xhc_transfer_set_configuration(general_usb_dev_t *dev, u32 config_value);
void xhc_transfer_set_interface(general_usb_dev_t *dev, u8 interface, u8 alter_set);
void get_idle(general_usb_dev_t *dev, void *buf, u32 bsize);
void get_protocol(general_usb_dev_t *dev, void *buf, u32 bsize);
void get_report(general_usb_dev_t *dev, void *buf, u32 bsize);

/* 获取描述符 */
cfig_desc_t *get_configuration_desc(device_descriptor_m *tree, int config_number);
interface_desc_t *get_interface_desc(void *configuration, int interface_number);
ep_desc_t *get_endpoint_desc(cfig_desc_t *cfig_desc, void *interface, int ep_number);

/* interface init 方法 */
void USB_HID_interface_init(general_usb_dev_t *dev, void *interface_desc);

#endif