#ifndef __HBA_H__
#define __HBA_H__

#include <common/type.h>
#include <rdix/pci.h>
#include <common/list.h>
#include <rdix/ata.h>

#define HBA_CC 0x010601

#define SATA_DEV_SIG 0x0101

/* 寄存器的地址单位是字节，要转换为单位为 u32 才能在 io_base 中正常访问 */
#define REG_IDX(addr) (addr >> 2)
#define HBA_REG_CAP 0x0
#define HBA_REG_GHC 0x4
#define HBA_REG_IS 0x8
#define HBA_REG_PI 0xc
#define HBA_REG_VS 0x10

#define VENDOR_SPECIFIC 0xa0

#define HBA_PORT_BASE 0x100
#define HBA_PORT_SIZE 0x80

/* 获取端口在 port->reg_base 中的索引 */
#define HBA_PORT_PxCLB 0x0
#define HBA_PORT_PxCLBU 0x4
#define HBA_PORT_PxFB 0x8
#define HBA_PORT_PxFBU 0xc
#define HBA_PORT_PxIS 0x10
#define HBA_PORT_PxIE 0x14
#define HBA_PORT_PxCMD 0x18
#define HBA_PORT_PxTFD 0x20
#define HBA_PORT_PxSIG 0x24
#define HBA_PORT_PxSSTS 0x28
#define HBA_PORT_PxSERR 0x30
#define HBA_PORT_PxCI 0x38

/* hba 全局寄存器位选择子 */
#define HBA_GHC_AE (1 << 31)
#define HBA_GHC_IE (1 << 1)
#define HBA_GHC_HR (1 << 0)

/* port 寄存器选择子 */
#define HBA_PORT_CMD_ST 1
#define HBA_PORT_CMD_FRE (1 << 4)
#define HBA_PORT_CMD_FR (1 << 14)
#define HBA_PORT_CMD_CR (1 << 15)
#define HBA_PORT_TFD_BSY (1 << 7)
#define HBA_PORT_IE_DPE (1 << 5)

/* fis 类型 */
#define FIS_RH2D 0x27

/* fis 选项字段默认值 */
#define FIS_DEFAULT_OPT 0x80

typedef enum send_status_t{
    SUCCESSFUL,
    NOT_SATA,
    _BUSY,
    GENERAL_ERROR,
} send_status_t;

struct hba_port_t;

/* 接在 hba 端口上的设备类型 */
typedef struct hba_dev_t{
    char sata_serial[21];
    char model[41];
    u32 flags;
    u64 max_lba;    //可访问的最大逻辑扇区数
    u32 block_size; //逻辑扇区大小
    u32 block_per_sec;  //每个物理扇区包含的逻辑扇区数
    u64 wwn;        //全球唯一标识符

    u8 spd;
    void *data;
    send_status_t last_status;

    struct hba_port_t *port;
} hba_dev_t;

/* port 命令列表中头部类型 */
typedef struct cmd_list_slot{
    u8 CFL : 5;
    u8 flag_pwa : 3;
    u8 flag_rcbr : 4;
    u8 PMP : 4;
    u16 PRDTL;
    u32 PRDBC;
    u8 reserved1 : 7;
    u32 CTBA : 25;
    u32 CTBAU;
    u32 reserved2[4];
} _packed cmd_list_slot;

typedef struct cmd_tab_item{
    /* dba 最后一位必须为 0（字对齐） */
    u32 dba;
    u32 dbau;
    u32 reserved;
    u32 dbc : 22;
    u32 resvl : 9;
    u32 i : 1;
} _packed cmd_tab_item;

typedef struct cmd_tab_t{
    u32 cfis[16];
    u32 acmd[4];
    u32 reserved[12];
    cmd_tab_item item[0];
} cmd_tab_t;

/* hba 端口类型 */
typedef struct hba_port_t{
    int32 port_num;
    /* 这里是命令列表和接受 fis 的存放位置的虚拟地址
     * 需要转换成物理地址才能填到对应的寄存器上 */
    cmd_list_slot *vPxCLB;
    u32 *vPxFB;

    u32 *reg_base;
} hba_port_t;

/* hba 设备 */
typedef struct hba_t{
    device_t* dev_info;
    u32 *io_base;
    List_t *devices;
    u8 per_port_slot_cnt;
} hba_t;

/* 会被外部改变的值需要加上 volatile
 * 比如这个结构体里的值会被 device 改变，编译器可能不知道
 * 他可能会把值存在寄存器里，当腰读取的时候去寄存器里读，而不是这个内存空间
 * 由此可能造成错误 */
typedef volatile struct tagHBA_FIS
{
	// 0x00
	FIS_DMA_SETUP	dsfis;		// DMA Setup FIS
	u8         pad0[4];
 
	// 0x20
	FIS_PIO_SETUP	psfis;		// PIO Setup FIS
	u8         pad1[12];
 
	// 0x40
	FIS_REG_D2H	rfis;		// Register – Device to Host FIS
	u8         pad2[4];
 
	// 0x58
	u8	    sdbfis[8];		// Set Device Bit FIS
 
	// 0x60
	u8         ufis[64];
 
	// 0xA0
	u8   	rsv[0x100-0xA0];
} HBA_FIS;

typedef u32 slot_num;

void hba_init();



#endif