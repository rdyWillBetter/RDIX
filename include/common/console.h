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
/* 默认颜色 */
#define WORD_TYPE_DEFAULT   0b00000111

#define COLOR_TYPE_BLINK 0x80
#define COLOR_TYPE_HIGHLIGHT 0x8

/* 前景色 */
#define COLOR_TYPE_FORE_BLACK 0x0
#define COLOR_TYPE_FORE_RED 0x4
#define COLOR_TYPE_FORE_GREEN 0x2
#define COLOR_TYPE_FORE_YELLOW 0x6
#define COLOR_TYPE_FORE_BLUE 0x1
#define COLOR_TYPE_FORE_PURPLE 0x5
#define COLOR_TYPE_FORE_CYAN 0x3
#define COLOR_TYPE_FORE_WHITE 0x7

/* 背景色 */
#define COLOR_TYPE_BACK_BLACK 0x0
#define COLOR_TYPE_BACK_RED (0x4 << 4)
#define COLOR_TYPE_BACK_GREEN (0x2 << 4)
#define COLOR_TYPE_BACK_YELLOW (0x6 << 4)
#define COLOR_TYPE_BACK_BLUE (0x1 << 4)
#define COLOR_TYPE_BACK_PURPLE (0x5 << 4)
#define COLOR_TYPE_BACK_CYAN (0x3 << 4)
#define COLOR_TYPE_BACK_WHITE (0x7 << 4)

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
u16 get_cursor_position();
void set_cursor_position(u16 cursor_position, bool clean);
u16 get_screen_position();
void set_screen_position(u16 screen_position);
void keyboard_arrow(Arrow_t arrow);

#endif