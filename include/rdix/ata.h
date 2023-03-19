#ifndef __ATA_H__
#define __ATA_H__

#include <common/type.h>

typedef enum ATA_CMD_TYPE{
	ATA_CMD_IDENTIFY_DEVICE = 0xec,
} SATA_CMD_TYPE;

typedef struct tagFIS_REG_H2D
{
	// DWORD 0
	u8  fis_type;	// FIS_TYPE_REG_H2D
 
	u8  pmport:4;	// Port multiplier
	u8  rsv0:3;		// Reserved
	u8  c:1;		// 1: Command, 0: Control
 
	u8  command;	// Command register
	u8  featurel;	// Feature register, 7:0
 
	// DWORD 1
	u8  lba0;		// LBA low register, 7:0
	u8  lba1;		// LBA mid register, 15:8
	u8  lba2;		// LBA high register, 23:16
	u8  device;		// Device register
 
	// DWORD 2
	u8  lba3;		// LBA register, 31:24
	u8  lba4;		// LBA register, 39:32
	u8  lba5;		// LBA register, 47:40
	u8  featureh;	// Feature register, 15:8
 
	// DWORD 3
	u8  countl;		// Count register, 7:0
	u8  counth;		// Count register, 15:8
	u8  icc;		// Isochronous command completion
	u8  control;	// Control register
 
	// DWORD 4
	u8  rsv1[4];	// Reserved
} _packed FIS_REG_H2D;

typedef struct tagFIS_REG_D2H
{
	// DWORD 0
	u8  fis_type;    // FIS_TYPE_REG_D2H
 
	u8  pmport:4;    // Port multiplier
	u8  rsv0:2;      // Reserved
	u8  i:1;         // Interrupt bit
	u8  rsv1:1;      // Reserved
 
	u8  status;      // Status register
	u8  error;       // Error register
 
	// DWORD 1
	u8  lba0;        // LBA low register, 7:0
	u8  lba1;        // LBA mid register, 15:8
	u8  lba2;        // LBA high register, 23:16
	u8  device;      // Device register
 
	// DWORD 2
	u8  lba3;        // LBA register, 31:24
	u8  lba4;        // LBA register, 39:32
	u8  lba5;        // LBA register, 47:40
	u8  rsv2;        // Reserved
 
	// DWORD 3
	u8  countl;      // Count register, 7:0
	u8  counth;      // Count register, 15:8
	u8  rsv3[2];     // Reserved
 
	// DWORD 4
	u8  rsv4[4];     // Reserved
} FIS_REG_D2H;

typedef struct tagFIS_PIO_SETUP
{
	// DWORD 0
	u8  fis_type;	// FIS_TYPE_PIO_SETUP
 
	u8  pmport:4;	// Port multiplier
	u8  rsv0:1;		// Reserved
	u8  d:1;		// Data transfer direction, 1 - device to host
	u8  i:1;		// Interrupt bit
	u8  rsv1:1;
 
	u8  status;		// Status register
	u8  error;		// Error register
 
	// DWORD 1
	u8  lba0;		// LBA low register, 7:0
	u8  lba1;		// LBA mid register, 15:8
	u8  lba2;		// LBA high register, 23:16
	u8  device;		// Device register
 
	// DWORD 2
	u8  lba3;		// LBA register, 31:24
	u8  lba4;		// LBA register, 39:32
	u8  lba5;		// LBA register, 47:40
	u8  rsv2;		// Reserved
 
	// DWORD 3
	u8  countl;		// Count register, 7:0
	u8  counth;		// Count register, 15:8
	u8  rsv3;		// Reserved
	u8  e_status;	// New value of status register
 
	// DWORD 4
	u16 tc;		// Transfer count
	u8  rsv4[2];	// Reserved
} FIS_PIO_SETUP;

typedef struct tagFIS_DMA_SETUP
{
	// DWORD 0
	u8  fis_type;	// FIS_TYPE_DMA_SETUP
 
	u8  pmport:4;	// Port multiplier
	u8  rsv0:1;		// Reserved
	u8  d:1;		// Data transfer direction, 1 - device to host
	u8  i:1;		// Interrupt bit
	u8  a:1;            // Auto-activate. Specifies if DMA Activate FIS is needed
 
        u8  rsved[2];       // Reserved
 
	//DWORD 1&2
 
        u64 DMAbufferID;    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
                                 // SATA Spec says host specific and not in Spec. Trying AHCI spec might work.
 
        //DWORD 3
        u32 rsvd;           //More reserved
 
        //DWORD 4
        u32 DMAbufOffset;   //Byte offset into buffer. First 2 bits must be 0
 
        //DWORD 5
        u32 TransferCount;  //Number of bytes to transfer. Bit 0 must be 0
 
        //DWORD 6
        u32 resvd;          //Reserved
 
} FIS_DMA_SETUP;

#endif