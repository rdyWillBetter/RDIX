/*配置实时时钟*/
#include <common/time.h>
#include <common/stdlib.h>
#include <common/type.h>
#include <common/io.h>
#include <common/interrupt.h>
#include <rdix/kernel.h>

//访问 cmos 的地址端口和数据端口
#define CMOS_CTRL 0x70
#define CMOS_DATA 0x71

//记录时间以及闹钟的寄存器
#define SEC_ADDR 0x00
#define ALARM_SEC_ADDR 0x01
#define MIN_ADDR 0x02
#define ALARM_MIN_ADDR 0x03
#define HOUR_ADDR 0x04
#define ALARM_HOUR_ADDR 0x05
#define WDAY_ADDR 0x06
#define MDAY_ADDR 0x07
#define MON_ADDR 0x08
#define YEAR_ADDR 0x09

//四个控制字用于设置中断
//RTC有三种中断，开启哪一种以及触发频率由四个控制器确定
#define REG_A_ADDR 0x0a
#define REG_B_ADDR 0x0b
#define REG_C_ADDR 0x0c
#define REG_D_ADDR 0x0d

static time_t month_days[] = {
    0,
    0,
    31,
    31 + 28,
    31 + 28 + 31,
    31 + 28 + 31 + 30,
    31 + 28 + 31 + 30 + 31,
    31 + 28 + 31 + 30 + 31 + 30,
    31 + 28 + 31 + 30 + 31 + 30 + 31,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30
};

static u8 read_cmos(u8 addr){
    port_outb(CMOS_CTRL, addr);
    return port_inb(CMOS_DATA);
}

void write_cmos(u8 addr, u8 data){
    port_outb(CMOS_CTRL, addr);
    port_outb(CMOS_DATA, data);
}

void time_read(time_b *tm){
    while (read_cmos(REG_A_ADDR) & 0x80); //寄存器第 1 位为 0 时表示cmos目前是可读的。

    tm->tm_sec = bcd2bin(read_cmos(SEC_ADDR)); //直接读取的是bcd码
    tm->tm_min = bcd2bin(read_cmos(MIN_ADDR));
    tm->tm_hour = bcd2bin(read_cmos(HOUR_ADDR));
    tm->tm_wday = bcd2bin(read_cmos(WDAY_ADDR));
    tm->tm_mday = bcd2bin(read_cmos(MDAY_ADDR));
    tm->tm_mon = bcd2bin(read_cmos(MON_ADDR));
    tm->tm_year = bcd2bin(read_cmos(YEAR_ADDR));
}

time_t mktime(time_b *tm){
    time_t sec = 0;

    //1972 年是闰年
    sec += (tm->tm_year + 30) * (3600 * 24 * 365); //从 1970 到现在的年份一共过了多少天
    sec += ((tm->tm_year + 30 + 1) / 4) * (3600 * 24); //加上 1970 到现在的闰年的天数

    sec += (month_days[tm->tm_mon] + tm->tm_mday) * (3600 * 24);

    if ((tm->tm_year + 30 + 2) % 4 == 0 && tm->tm_mon > 2){
        sec += 3600 * 24;
    }

    return sec;
}

static void rtc_handler(u32 int_num, u32 code){
    time_b tm;
    time_read(&tm);

    printk("time %d/%d/%d  %d:%d:%d, is new 4\n",\
    tm.tm_year,\
    tm.tm_mon,\
    tm.tm_mday,\
    tm.tm_hour,\
    tm.tm_min,\
    tm.tm_sec);

    read_cmos(REG_C_ADDR); //读一下 C 寄存器，使其清零。不清空 C 就无法继续触发中断
    lapic_send_eoi();
}

void rtc_init(){
    //write_cmos(REG_A_ADDR, 0b0101110); 

    write_cmos(REG_B_ADDR, read_cmos(REG_B_ADDR) | 0x10);//打开更新周期中断，1 秒一次
    read_cmos(REG_C_ADDR);

    install_int(IRQ8_RTC, 0, 0, rtc_handler);
}