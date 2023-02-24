#include <common/console.h>
#include <common/io.h>
#include <common/type.h>
#include <common/string.h>
#include <common/interrupt.h>
#include <common/assert.h>
#include <rdix/kernel.h>

/* 值为 0 的全局变量和未初始化的全局变量是一样的，都是放在 bss 段。值都是随机的
 * 因此这样的全局变量一定要初始化后才能使用 */
static u16 CPosition = 0; //每次字符显示的位置，光标位置
static u16 SPosition; //屏幕位置

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

void set_cursor_position(u16 cursor_position){
    if (cursor_position >= SPosition + SCREEN_ROW_COUNT * SCREEN_COL_COUNT){
        cursor_position = scroll_up();
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

    set_screen_position(SPosition);
    set_cursor_position(CPosition);

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

    set_cursor_position(CPosition);
}

/* 已做关中断处理，可放心使用 */
void console_put_char(char ch, u8 type){
    u16 *vedio = (u16 *)VEDIO_BASE_ADDR;
    u16 word_block = (type << 8) | ch;

    bool IF_stat = get_IF();
    set_IF(false);
    
    switch (ch)
    {
        case CLR_LF:
            proc_lf();
            break;
        
        case BACKSPASE:
            --CPosition;
            vedio[CPosition] = BLANK;
            set_cursor_position(CPosition);
            break;
        
        default:
            vedio[CPosition] = word_block;
            ++CPosition;
            set_cursor_position(CPosition);
            break;
    }

    set_IF(IF_stat);
}

/* =============================================
 * bug 调试记录
 * console_put_string 是调用了一系列的 console_put_char
 * 意味着每个 console_put_char 应当连续执行。如果中间由于竞争问题执行了其他任务的 console_put_char
 * 就会导致打印的字符串不连续，所以 console_put_string 是一个互斥事件
 * ============================================= */
void console_put_string(const char* str, u8 type){
    bool IF_stat = get_IF();
    set_IF(false);

    for (int i = 0; str[i] != '\0'; ++i){
        console_put_char(str[i], type);
    }

    set_IF(IF_stat);
}

u16 scroll_up(){
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
        //注意CPosition的修改，函数耦合过高，待优化
    }
    
    set_screen_position(SPosition);
    
    u16 *earse = (u16 *)(VEDIO_BASE_ADDR + (SPosition + FULL_SCREEN_SIZE - SCREEN_ROW_COUNT) * 2);
    for (int i = 0; i < SCREEN_ROW_COUNT; ++i){
        earse[i] = BLANK;
    }

    return CPosition;
}