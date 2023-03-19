#ifndef __IO_H__
#define __IO_H__

#include <common/type.h>

/* in 将数据从设备寄存器提取到 cpu */
extern u8 port_inb(u16 port);
extern u16 port_inw(u16 port);
extern u32 port_ind(u16 port);

/* out 将数据 data 从 cpu 输出到设备寄存器 */
extern void port_outb(u16 port, u8 data);
extern void port_outw(u16 port, u16 data);
extern void port_outd(u16 port, u32 data);

void iodelay();

#endif