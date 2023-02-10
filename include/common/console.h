#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <common/type.h>

#define CRT_ADDR_REG 0x3d4
#define CRT_DATA_REG 0x3d5

#define CRT_CURSOR_H 0xe //光标位置高位，长度为1B
#define CRT_CURSOR_L 0xf //光标位置低位，长度为1B
#define CRT_SCREEN_H 0xc //显示器开始位置高位，距0xb8000的偏移量，长度为1B
#define CRT_SCREEN_L 0xd //显示器开始位置低位，长度为1B

#define VEDIO_BASE_ADDR 0xb8000
#define VEDIO_END_ADDR 0xc0000
#define SCREEN_ROW_COUNT 80
#define SCREEN_COL_COUNT 25 //反了
#define FULL_SCREEN_SIZE (SCREEN_ROW_COUNT * SCREEN_COL_COUNT)
#define BLANK 0x0720

#define WORD_TYPE_DEFAULT 7 //灰白色

#define CLR_LF '\n'
#define BACKSPASE '\b'

void console_init();
void console_clean();
u16 get_cursor_position();
void set_cursor_position(u16 cursor_position);
u16 get_screen_position();
void set_screen_position(u16 screen_position);
void console_put_char(char ch, u8 type);
void console_put_string(const char* str, u8 type);
u16 scroll_up(u16 cursor_position);

#endif