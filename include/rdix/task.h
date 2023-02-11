#ifndef __TASK_H__
#define __TASK_H__

#include <common/type.h>
#include <common/bitmap.h>
#include <common/list.h>

#define TASK_NAME_LEN 16    //任务名长度，单位 u32
#define KERNEL_TCB 0x18000
/* 任务函数句柄 */
typedef void (*task_program)(void);

/* 进程状态 */
typedef enum task_state_t
{
    TASK_INIT,     // 初始化
    TASK_RUNNING,  // 执行
    TASK_READY,    // 就绪
    TASK_BLOCKED,  // 阻塞
    TASK_SLEEPING, // 睡眠
    TASK_WAITING,  // 等待
    TASK_DIED,     // 死亡
} task_state_t;

/* TCB 数据结构 */
typedef struct TCB_t{
    void *stack;             // 任务栈指针的指针
    task_state_t state;      // 任务状态
    u32 priority;            // 任务优先级
    u32 ticks;               // 剩余时间片
    u32 jiffies;             // 上次执行时全局时间片，也就是总时间
    char name[TASK_NAME_LEN]; // 任务名
    u32 uid;                 // 用户 id
    u32 pde;                 // 页目录物理地址
    bitmap_t *vmap;   // 进程虚拟内存管理位图
    u32 magic;               // 内核魔数，用于检测栈溢出
} TCB_t;

/* 进程切换时需要保存一系列寄存器。
 * 该数据结构是为了在首次切换到该进程时保持栈平衡。 */
typedef struct task_stack_t{
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
    void (*eip)(void);
} task_stack_t;

void task_init(void);
ListNode_t *current_task();
void schedule();
char *task_name();
ListNode_t *task_create(task_program handle, const char *name, u32 priority, u32 uid);
void block(ListNode_t *task);
void unblock(ListNode_t *task);
void task_sleep(time_t time);
void weakup();

#endif