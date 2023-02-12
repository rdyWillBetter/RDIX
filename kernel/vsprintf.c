#include <common/stdarg.h>
#include <common/type.h>
#include <common/string.h>

#define ZEROPAD 1  // 填充零
#define SIGN 2     // 带符号数
#define PLUS 4     // 显示加
#define SPACE 8    // 如果第一个字符不是正负号，则在其前面加上一个空格
#define LEFT 16    // 左调整
#define SPECIAL 32 // 0x
#define SMALL 64   // 使用小写字母

bool is_digit(const char ch){
    if (ch >= '0' && ch <= '9') return true;
    return false;
}

static int skip_atoi(const char **s){
    int i = 0;
    while (is_digit(**s)){
        i = i * 10 + **s - '0';
        ++(*s);
    }
    return i;
}

// 将整数转换为指定进制的字符串
// str - 输出字符串指针
// num - 整数
// base - 进制基数
// size - 字符串长度
// precision - 数字长度(精度)
// flags - 选项
static char *number(char *str, unsigned long num, int base, int size, int precision, int flags){
    int i = 0;
    char tmp[36], c = 0, sign = 0; // c 为填充的字符
    char *digit = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char *small = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (flags & SMALL){
        digit = small;
    }

    if (flags & LEFT){
        flags &= ~ZEROPAD; //如果是左对齐就不需要填0
    }

    if (base < 2 || base > 36)
        return 0;

    c = (flags & ZEROPAD) ? '0' : ' ';

    if (flags & SIGN && num < 0){ //没有 SIGN 标志就意味着不可能有负数传进来
        sign = '-';
        num = -num;
    }
    else{
        sign = (flags & PLUS) ? '+' : ((flags & SPACE) ? ' ' : 0);
    }

    if (sign) size--;

    if (flags & SPECIAL){
        if (base == 16) size -= 2; //在前面放置 0x
        else if (base == 8) size -= 1; //在前面放 0
    }

    if (!num){
        tmp[i++] = '0';
    }

    while (num){
        tmp[i++] = digit[num % base];
        num = num / base;
    }

    //if (i > precision)
    //    precision = i;
    /*===========================================================================*/
    size -= i;

    if (!(flags & (ZEROPAD | LEFT)))
        while (size-- > 0)
            *str++ = ' ';

    if (sign)
        *str++ = sign;

    if (flags & SPECIAL){
        *str++ = '0';
        if (base == 16){
            *str++ = 'x';
        }
    }

    if (!(flags & LEFT))
        while (size-- > 0)
            *str++ = c;

    --i;
    while (i >= 0){
        *str++ = tmp[i--];
    }

    while (size-- > 0)
        *str++ = ' ';

    return str;
}

//格式控制字符串=>%[flags][width][.prec][length]specifier
//其中[]为可选，<>为必要选项
//使用时需要关中断，防止竞争
int vsprintf(char *buf, const char *fmt, va_list arg){

    char *str = buf, *s;

    int flags;
    int field_width;
    int precision;
    int qualifier;

    while (*fmt != '\0'){
        if (*fmt != '%'){
            *str++ = *fmt++;
            continue;
        }
        /*complement [flags]*/
        flags = 0;
    repeat:
        ++fmt;
        switch (*fmt){
            case '-':   flags |= LEFT; goto repeat;
            case '+':   flags |= PLUS; goto repeat;
            case ' ':   flags |= SPACE; goto repeat;
            case '#':   flags |= SPECIAL; goto repeat;
            case '0':   flags |= ZEROPAD; goto repeat;
        }

        /*complement [width]*/
        field_width = -1;
        if (is_digit(*fmt))
            field_width = skip_atoi(&fmt);

        /*complement [.prec]*/
        precision = -1;
        if (*fmt == '.'){
            ++fmt;
            if (is_digit(*fmt))
                precision = skip_atoi(&fmt);

            if (precision < 0){
                precision = 0;
            }
        }

        /*complement [length]*/
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L'){
            qualifier = *fmt;
            ++fmt;
        }

        /*complement specifier*/
        switch (*fmt)
        {
        case 'd':
        case 'i':
            flags |= SIGN;
            str = number(str, va_arg(arg, u32), 10, field_width, precision, flags);
            break;
        case 'p':
            if (field_width == -1){
                field_width = 8;
                flags |= ZEROPAD;
                if (flags & SPECIAL)
                    field_width += 2;
            }
            str = number(str, va_arg(arg, u32), 16, field_width, precision, flags);
            break;
        case 'o':
            str = number(str, va_arg(arg, u32), 8, field_width, precision, flags);
            break;
        case 'x':
            flags |= SMALL;
        case 'X':
            str = number(str, va_arg(arg, u32), 16, field_width, precision, flags);
            break;
        case 's':
            s = va_arg(arg, char *);
            while (*s) *str++ = *s++;
            break;
        case 'c':
            *str++ = va_arg(arg, char);
            break;
        default:
            break;
        }
        ++fmt;
    }
    *str = '\0';
    return str - buf;
}