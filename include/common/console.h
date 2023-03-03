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

/* 颜色属性
 *     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * 含义  闪烁 |  R  |  G  |  B  | 高亮 |  R |   G  |  B  |
 *           |      背景        |      |       前景     | */
#define WORD_TYPE_DEFAULT   0b00000111 //灰白色
#define WORD_TYPE_DEBUG     0b00001001
#define WORD_TYPE_PANIC     0b10001100
#define WORD_TYPE_LOG       0b00011111
#define WORD_TYPE_ASSERT    0b01001111

/* 一个 tab 所占的空格数 */
#define TABLE_SIZE 4

#define CLR_LF '\n'
#define BACKSPASE '\b'
#define TABLE '\t'

typedef enum Arrow_t{
    UP,
    DOWN,
    LEFT,
    RIGHT,
} Arrow_t;

void console_init();
void console_clean();
u16 get_cursor_position();
void set_cursor_position(u16 cursor_position, bool clean);
u16 get_screen_position();
void set_screen_position(u16 screen_position);
void console_put_char(char ch, u8 type);
void console_put_string(const char* str, u8 type);
void keyboard_arrow(Arrow_t arrow);

#endif