#include <common/console.h>
#include <common/io.h>
#include <common/type.h>
#include <common/string.h>
#include <common/interrupt.h>
#include <common/assert.h>
#include <rdix/kernel.h>

/* 值为 0 的全局变量和未初始化的全局变量是一样的，都是放在 bss 段。值都是随机的
 * 因此这样的全局变量一定要初始化后才能使用 */
static u16 CPosition; //每次字符显示的位置，光标位置
static u16 SPosition; //屏幕位置

char _color;

enum GENERAL_CONTRAL{
    __RESET = 0,
    __HIGHLIGHT = 1,
    __DIM = 2,
    __BLINK = 5,
};

enum FORECOLOR{
    _FORE__BLACK = 30,
    _FORE__RED,
    _FORE__GREEN,
    _FORE__YELLOW,
    _FORE__BLUE,
    _FORE__PURPLE,
    _FORE__CYAN,
    _FORE__WHITE,
};

enum BACKCOLOR{
    _BACK__BLACK = 40,
    _BACK__RED,
    _BACK__GREEN,
    _BACK__YELLOW,
    _BACK__BLUE,
    _BACK__PURPLE,
    _BACK__CYAN,
    _BACK__WHITE,
};

/* 当前可访问的屏幕空间 */
static u16 end;


extern int skip_atoi(const char **s);

char *_color_proc(const char *fmt){
    char *str = fmt;
    char colorfmt[8];
    char *colorfmt_ptr = colorfmt;
    u8 pre_color = _color;

    if (*str != '\033')
        goto failed;
    if (*(++str) != '[')
        goto failed;

    for (size_t i = 0; *(++str) != ']' && i < 8; ++colorfmt_ptr, ++i){
        *colorfmt_ptr = *str;
    }

    if (*str != ']')
        goto failed;

    ++str;

    *colorfmt_ptr = '\0';

    colorfmt_ptr = colorfmt;

    u8 gen_code = 0;
    u8 fore_code = 0;
    u8 back_code = 0;

    gen_code = skip_atoi(&colorfmt_ptr);

    switch (gen_code){
        case __RESET: _color = WORD_TYPE_DEFAULT; break;
        case __HIGHLIGHT: _color |= COLOR_TYPE_HIGHLIGHT; break;
        case __DIM: _color &= ~COLOR_TYPE_HIGHLIGHT; break;
        case __BLINK: _color |= COLOR_TYPE_BLINK; break;
        default: return fmt;
    }

    if (*colorfmt_ptr == '\0')
        return str;
    if (*colorfmt_ptr++ != ';')
        goto failed;

    fore_code = skip_atoi(&colorfmt_ptr);
    _color &= 0xf8;
    switch (fore_code){
        case _FORE__BLACK: _color |= COLOR_TYPE_FORE_BLACK; break;
        case _FORE__RED: _color |= COLOR_TYPE_FORE_RED; break;
        case _FORE__GREEN: _color |= COLOR_TYPE_FORE_GREEN; break;
        case _FORE__YELLOW: _color |= COLOR_TYPE_FORE_YELLOW; break;
        case _FORE__BLUE: _color |= COLOR_TYPE_FORE_BLUE; break;
        case _FORE__PURPLE: _color |= COLOR_TYPE_FORE_PURPLE; break;
        case _FORE__CYAN: _color |= COLOR_TYPE_FORE_CYAN; break;
        case _FORE__WHITE: _color |= COLOR_TYPE_FORE_WHITE; break;
        default: return fmt;
    }

    if (*colorfmt_ptr == '\0')
        return str;
    if (*colorfmt_ptr++ != ';')
        goto failed;

    back_code = skip_atoi(&colorfmt_ptr);
    _color &= 0x8f;
    switch (back_code){
        case _BACK__BLACK: _color |= COLOR_TYPE_BACK_BLACK; break;
        case _BACK__RED: _color |= COLOR_TYPE_BACK_RED; break;
        case _BACK__GREEN: _color |= COLOR_TYPE_BACK_GREEN; break;
        case _BACK__YELLOW: _color |= COLOR_TYPE_BACK_YELLOW; break;
        case _BACK__BLUE: _color |= COLOR_TYPE_BACK_BLUE; break;
        case _BACK__PURPLE: _color |= COLOR_TYPE_BACK_PURPLE; break;
        case _BACK__CYAN: _color |= COLOR_TYPE_BACK_CYAN; break;
        case _BACK__WHITE: _color |= COLOR_TYPE_BACK_WHITE; break;
        default: return fmt;
    }

    return str;
    
failed:
    _color = pre_color;
    return fmt;
}

u16 get_cursor_position(){

    u16 cursor_position = 0;
    port_outw(CRT_ADDR_REG,CRT_CURSOR_H);
    cursor_position = port_inb(CRT_DATA_REG) << 8;
    port_outw(CRT_ADDR_REG,CRT_CURSOR_L);
    cursor_position |= port_inb(CRT_DATA_REG);

    return cursor_position;
}

u16 get_screen_position(){

    u16 screen_position = 0;
    port_outw(CRT_ADDR_REG,CRT_SCREEN_H);
    screen_position = port_inb(CRT_DATA_REG) << 8;
    port_outw(CRT_ADDR_REG,CRT_SCREEN_L);
    screen_position |= port_inb(CRT_DATA_REG);

    return screen_position;
}

static u16 scroll_down(bool clean);
static void scroll_up();

void set_cursor_position(u16 cursor_position, bool clean){
    if (cursor_position >= SPosition + SCREEN_ROW_COUNT * SCREEN_COL_COUNT){
        cursor_position = scroll_down(clean);
    }

    if (cursor_position < SPosition){
        scroll_up();
    }

    port_outb(CRT_ADDR_REG,CRT_CURSOR_H);
    port_outb(CRT_DATA_REG,cursor_position >> 8);

    port_outb(CRT_ADDR_REG,CRT_CURSOR_L);
    port_outb(CRT_DATA_REG,cursor_position);
}

void set_screen_position(u16 screen_position){
    assert(!(screen_position % 80));

    port_outb(CRT_ADDR_REG,CRT_SCREEN_H);
    port_outb(CRT_DATA_REG,screen_position >> 8);

    port_outb(CRT_ADDR_REG,CRT_SCREEN_L);
    port_outb(CRT_DATA_REG,screen_position); 
}

void console_clean(){
    CPosition = 0;
    SPosition = 0;
    _color = WORD_TYPE_DEFAULT;

    end = 0;

    set_screen_position(SPosition);
    set_cursor_position(CPosition, false);

    u16 *vedio = (u16 *)VEDIO_BASE_ADDR;
    for (u16 i = 0; i < FULL_SCREEN_SIZE; ++i){
        vedio[i] = BLANK;
    }
}

void console_init(){
    console_clean();
}

void proc_lf(){
    u16 next_row = CPosition / SCREEN_ROW_COUNT + 1;
    CPosition = next_row * SCREEN_ROW_COUNT;

    set_cursor_position(CPosition, true);
}

void proc_table(){
    CPosition = ((CPosition + TABLE_SIZE) / TABLE_SIZE) * TABLE_SIZE;

    set_cursor_position(CPosition, true);
}

/* 已做关中断处理，可放心使用 */
void console_put_char(char ch){
    u16 *vedio = (u16 *)VEDIO_BASE_ADDR;
    u16 word_block = (_color << 8) | ch;

    bool IF_stat = get_IF();
    set_IF(false);
    
    switch (ch)
    {
        case CLR_LF:
            proc_lf();
            break;
        
        case TABLE:
            proc_table();
            break;
        
        case BACKSPASE:
            --CPosition;
            vedio[CPosition] = BLANK;
            set_cursor_position(CPosition, false);
            break;
        
        default:
            vedio[CPosition] = word_block;
            ++CPosition;
            set_cursor_position(CPosition, true);
            break;
    }

    end = CPosition;

    set_IF(IF_stat);
}

/* =============================================
 * bug 调试记录
 * console_put_string 是调用了一系列的 console_put_char
 * 意味着每个 console_put_char 应当连续执行。如果中间由于竞争问题执行了其他任务的 console_put_char
 * 就会导致打印的字符串不连续，所以 console_put_string 是一个互斥事件
 * ============================================= */
void console_put_string(const char* str){
    bool IF_stat = get_IF();
    set_IF(false);

    while (*str != '\0'){
        while (*str == '\033' && *(str + 1) == '[')
            str = _color_proc(str);
        
        console_put_char(*str++);
    }

    _color = WORD_TYPE_DEFAULT;

    set_IF(IF_stat);
}

/* clean == true 时往下滚屏后需要擦除下一行  */
static u16 scroll_down(bool clean){
    if (VEDIO_BASE_ADDR + (SPosition + FULL_SCREEN_SIZE) * 2 \
     < VEDIO_END_ADDR - SCREEN_ROW_COUNT * 4){
        SPosition += SCREEN_ROW_COUNT;
    }
    else{
        memcpy((void *)VEDIO_BASE_ADDR, \
        (const void *)(VEDIO_BASE_ADDR + (SPosition + SCREEN_ROW_COUNT) * 2), \
        (size_t)((FULL_SCREEN_SIZE - SCREEN_ROW_COUNT) * 2));
        
        SPosition = 0;
        CPosition = FULL_SCREEN_SIZE - SCREEN_ROW_COUNT;
        end = CPosition;
        //注意CPosition的修改，函数耦合过高，待优化
    }
    
    set_screen_position(SPosition);
    
    /* ===========================================================================
     * bug 调试记录
     * 如果没有 clean 标志位，这里在使用键盘上的上下左右箭头向下滚屏时，会误删掉一些内容
     * 所以需要 clean 位标明在向下滚屏时是否删除屏幕最后一行
     * =========================================================================== */
    if (clean){
        u16 *earse = (u16 *)(VEDIO_BASE_ADDR + (SPosition + FULL_SCREEN_SIZE - SCREEN_ROW_COUNT) * 2);
        for (int i = 0; i < SCREEN_ROW_COUNT; ++i){
            earse[i] = BLANK;
        }
    }

    return CPosition;
}

static void scroll_up(){
    assert(!(SPosition % SCREEN_ROW_COUNT));

    if (SPosition > 0)
        SPosition -= SCREEN_ROW_COUNT;

    set_screen_position(SPosition);
}

void keyboard_arrow(Arrow_t arrow){
    switch (arrow){
        case DOWN: if (CPosition + SCREEN_ROW_COUNT <= end){
            CPosition += SCREEN_ROW_COUNT;
        } else{
            CPosition = end;
        }
        set_cursor_position(CPosition, false);
        break;

        case UP: if (CPosition - SCREEN_ROW_COUNT >= 0){
            CPosition -= SCREEN_ROW_COUNT;
            set_cursor_position(CPosition, false);
        } break;
        
        case LEFT: if (CPosition - 1 >= 0){
            --CPosition;
            set_cursor_position(CPosition, false);
        } break;

        case RIGHT: if (CPosition + 1 <= end){
            ++CPosition;
            set_cursor_position(CPosition, false);
        } break;

        default: break;
    }
}