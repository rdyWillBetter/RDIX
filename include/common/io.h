#ifndef __IO_H__
#define __IO_H__

#include <common/type.h>

extern u8 port_inb(u16 port);
extern u16 port_inw(u16 port);

extern void port_outb(u16 port, u8 data);
extern void port_outw(u16 port, u16 data);

#endif