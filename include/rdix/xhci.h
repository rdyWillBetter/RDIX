#ifndef __USB_H__
#define __USB_H__

#include <common/type.h>
#include <rdix/pci.h>

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
#define PortStatusChangeEvent 34
#define GetEnableSlot 9
#define CmdCompletion 33

struct cmd_ring_t{
    u32 *trb;
    bool PCS;
};

struct event_ring_t{
    u32 *trb;
    bool CCS;
};

typedef struct xhc_t{
    pci_device_t* pci_info;
    u32 *cap_base;
    u32 *op_base;
    u32 *run_base;
    u32 *doorbell;
    struct cmd_ring_t cmd_ring;
    struct event_ring_t event_ring;

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

#endif