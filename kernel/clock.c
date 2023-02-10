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

/* jiffies 只是用来表示任务执行的先后顺序，初值不重要 */
static int jiffies;
static void clock_handler(u32 int_num, u32 code){
    TCB_t *current = (TCB_t *)(current_task()->owner);

    /* out of memory */
    assert(current->magic == RDIX_MAGIC);

    /* 不发送 eoi 的话下次外中断会被屏蔽 */
    sent_eoi(int_num);

    current->jiffies = ++jiffies;

    if (--current->ticks == 0){
        current->ticks = current->priority;
        schedule();
    }
}

void clock_init(){
    pit_init();
    set_int_handler(IRQ0_COUNTER, clock_handler);
    set_int_mask(IRQ0_COUNTER, true);
}