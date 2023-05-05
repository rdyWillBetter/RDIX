#ifndef __TASK_H__
#define __TASK_H__

#include <common/type.h>
#include <common/bitmap.h>
#include <common/list.h>
#include <rdix/memory.h>
#include <fs/fs.h>

#define KERNEL_UID 0
#define USER_UID 3

#define TASK_NAME_LEN 16    //任务名长度
#define TASK_PWD_LEN 1024

/* 任务函数句柄 */
typedef void (*task_program)(void);
typedef void (*user_target_t)(void);

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
    int status;             // 用于返回给父进程
    u32 priority;            // 任务优先级
    u32 ticks;               // 剩余时间片
    u32 jiffies;             // 上次执行时全局时间片，也就是总时间
    char name[TASK_NAME_LEN]; // 任务名
    u32 uid;                 // 用户 id
    u32 gid;
    pid_t pid;              // 当前任务id
    pid_t ppid;             // 父任务id
    pid_t waitpid;          // 等待进程号位 pid 的子进程释放
    page_entry_t *pde;                 // 页目录物理地址
    bitmap_t *vmap;   // 进程虚拟内存管理位图
    m_inode *i_root;    //根目录，用于绝对路径寻址
    m_inode *i_pwd;     //当前目录，用于相对路径寻址
    char *pwd;
    file_t *files[TASK_FILE_NR];
    u32 brk;
    u16 umask;
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

/* 用户程序在内核态的栈，用于从内核态切换到用户态 */
typedef struct intr_frame_t
{
    u32 edi;
    u32 esi;
    u32 ebp;
    // 虽然 pushad 把 esp 也压入，但 esp 是不断变化的，所以会被 popad 忽略
    u32 esp_dummy;

    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;

    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;

    u32 vector0;

    u32 error;

    /* 在内核态切换到用户态时，以下这五个会自动恢复 */
    u32 eip;
    u32 cs;
    u32 eflags;
    u32 esp;
    u32 ss;
} intr_frame_t;

void task_init(void);
ListNode_t *current_task();
void schedule();
char *task_name();
ListNode_t *task_create(task_program handle, void * param,  const char *name, u32 priority, u32 uid);
void block(List_t *list, ListNode_t *task, task_state_t task_state);
void unblock(ListNode_t *task);
void task_sleep(time_t time);
void weakup();
void user_task_create(user_target_t target, const char *name, u32 priority);
pid_t sys_waitpid(pid_t pid, int32 *status);

void kernel_task_create(user_target_t target, const char *name, u32 priority);
void user_task_create(user_target_t target, const char *name, u32 priority);
void kernel_thread_kill(ListNode_t *th);

#endif