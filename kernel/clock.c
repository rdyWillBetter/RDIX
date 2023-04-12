/*配置系统计数器*/
#include <common/clock.h>
#include <common/interrupt.h>
#include <common/io.h>
#include <common/assert.h>
#include <rdix/kernel.h>
#include <rdix/task.h>

static void pit_init(){
    port_outb(CONTROL_R, 0b00110100);
    port_outb(COUNTER_0, (u8)CLOCK_COUNTER);
    port_outb(COUNTER_0, (u8)(CLOCK_COUNTER >> 8));
}

/* jiffies 表示当前已经执行了多少个时间片 */
time_t jiffies;
static void clock_handler(u32 int_num, u32 code){
    /* 不发送 eoi 的话下次外中断会被屏蔽 */
    //sent_eoi(int_num);
    lapic_send_eoi();

    if (current_task() == NULL){
        schedule();
        return;
    }
        
    TCB_t *current = (TCB_t *)(current_task()->owner);

    /* out of memory */
    assert(current->magic == RDIX_MAGIC);

    current->jiffies = ++jiffies;

    weakup();

    if (--current->ticks == 0){
        current->ticks = current->priority;
        schedule();
    }
}

void clock_init(){
    jiffies = 0;
    pit_init();
    
    install_int(IRQ0_COUNTER, 0, 0, clock_handler);
}