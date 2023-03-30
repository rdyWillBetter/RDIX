#include <rdix/task.h>
#include <rdix/memory.h>
#include <rdix/kernel.h>
#include <common/string.h>
#include <common/assert.h>
#include <common/list.h>
#include <common/clock.h>
#include <common/global.h>
#include <common/interrupt.h>

#define TASK_NUM 64

extern bitmap_t v_bit_map;
extern time_t jiffies;
extern tss_t tss;

static u32 task_cnt;

List_t *block_list;
List_t *sleep_list;
List_t *ready_list;

static ListNode_t *running_task;

extern void interrupt_exit();

/* 通过 _ofp 修饰取消栈针
 * 如果不取消栈针，在函数退出时会修改 esp (mov esp, ebp)
 * 这是致命的 */
/* 并且该函数内不能有任何局部变量的声明
 * 局部变量的声明会导致函数在返回时改变 esp 的值（默认调用规范中是由被调用者恢复栈） */
_ofp void task_switch(TCB_t *current_task_src, TCB_t *next_task_src){
    asm volatile(
        "pushl %%ebp\n"
        "pushl %%ebx\n"
        "pushl %%esi\n"
        "pushl %%edi\n"

        "movl %%esp, (%1)\n"

        "movl (%0), %%esp\n"

        "popl %%edi\n"
        "popl %%esi\n"
        "popl %%ebx\n"
        "popl %%ebp\n"
        "sti\n" //调度结束后一定要记得开外中断
        :
        :"c"(next_task_src),"d"(current_task_src)
        :
    );
}

/* 做其他工作之前，要创建内核自己的 TCB */
static void kernel_task_init(){
    TCB_t *kernel = (TCB_t *)KERNEL_TCB;
    void *stack = NULL;

    block_list = new_list();
    sleep_list = new_list();
    ready_list = new_list();

    kernel->state = TASK_RUNNING;
    kernel->priority = 3;
    kernel->ticks = kernel->priority;
    kernel->jiffies = 0;
    strcpy((char *)kernel->name, "kernel");
    kernel->uid = 0;
    kernel->pde = 0x1000; //内核页目录，修改过memory.c后要注意这里可能出问题
    kernel->vmap = &v_bit_map; //内核虚拟内存位图，同上
    kernel->brk = KERNEL_MEMERY_SIZE;
    kernel->magic = RDIX_MAGIC;

    /* 开始执行 */
    running_task = new_listnode(kernel, kernel->priority);

    ++task_cnt;
}

ListNode_t *current_task(){
    return running_task;
}

char *task_name(){
    return ((TCB_t *)running_task->owner)->name;
}

ListNode_t *task_create(task_program handle, void *param, const char *name, u32 priority, u32 uid){
    TCB_t *tcb = (TCB_t *)alloc_kpage(1);
    task_stack_t *stack = (task_stack_t *)((u32)tcb + PAGE_SIZE - sizeof(task_stack_t));
    ListNode_t *node = new_listnode(tcb, priority);

    if (task_cnt >= TASK_NUM){
        PANIC("Can not create more task!\n");
    }

    ++task_cnt;
    
    /* edi 为指向传入参数结构体的指针 */
    stack->edi = (u32)param;
    stack->esi = 0x2;
    stack->ebx = 0x3;
    stack->ebp = 0x4;
    stack->eip = handle;

    tcb->pid = task_cnt;
    tcb->ppid = 0;
    tcb->stack = stack;
    tcb->state = TASK_READY;
    tcb->priority = priority;
    tcb->ticks = tcb->priority;
    tcb->jiffies = 0;
    strcpy((char *)tcb->name, name);
    tcb->uid = uid;
    tcb->pde = (page_entry_t *)0x1000; //内核页目录，修改过memory.c后要注意这里可能出问题
    tcb->vmap = &v_bit_map; //内核虚拟内存位图，同上
    tcb->brk = KERNEL_MEMERY_SIZE;
    tcb->magic = RDIX_MAGIC;

    list_push(ready_list, node);

    return node;
}

/* 目前没有好的调度算法
 * 就是排队而已 */
void schedule(){
    ListNode_t *next = list_popback(ready_list);
    ListNode_t *current = running_task;

    /* bug 调试记录
     * 必须要验证 next 不为 NULL 后，在能进行下一步操作 */
    if (next == NULL)
        return;

    TCB_t *next_tcb = (TCB_t *)next->owner;
    TCB_t *current_tcb = (TCB_t *)current->owner;

    running_task = next;

    assert(next_tcb->magic == RDIX_MAGIC);

    next_tcb->state = TASK_RUNNING;

    /* 调用 block 或 sleep 后当前任务很可能已经被加入到对应的链表中
     * 因此这里就不能再把它加入到 ready链表 中 */
    if (current->container == NULL){
        current_tcb->state = TASK_READY;
        list_push(ready_list, current);
    }

    if (next_tcb->pde != get_cr3()){
        set_cr3(next_tcb->pde);
    }

    if (next_tcb->uid == USER_UID){
        /* tss.esp0 应当是该用户任务的内核栈的初始地址，这里想一想有没有更好的办法 */
        /* =========================================================
         * bug 调试记录
         * 把 0xfffff000 写成 PAGE_SIZE 导致 esp0 = 0x1000, 修改到页目录
         * 真 90% 莫名其妙的问题都是栈引起的
         * ========================================================= */
        tss.esp0 = ((u32)next_tcb->stack & 0xfffff000) + PAGE_SIZE;
    }

    task_switch(current->owner, next->owner);
}

/* 参数存放在 edi 中 */
static void *kernel_to_user(){
    intr_frame_t iframe;
    /* target 为用户程序入口地址 */
    user_target_t *target;
    void *kernel_stack = &iframe;
    TCB_t *current = (TCB_t *)current_task()->owner;

    /* 参数指针保存在 edi 中 */
    asm volatile(
        "movl %%edi,%0\n"
        :"=m"(target)
    );

    /* 用户进程的虚拟内存空间位图以及页表待初始化 */
    
    current->vmap = (bitmap_t *)malloc(sizeof(bitmap_t));

    /* 用户进程的虚拟内存位图为 4KB，所管理的内存起始地址为 16M
     * 用户程序一共可使用的内存为 128M，也就是 16M 到 144M */
    bitmap_init(current->vmap, alloc_kpage(1), PAGE_SIZE, PAGE_IDX(KERNEL_MEMERY_SIZE));

    /* 分配栈空间 */
    for (page_idx_t i = PAGE_IDX(USER_STACK_BOTTOM); i < PAGE_IDX(USER_STACK_TOP); ++i){
        assert(bitmap_set(current->vmap, i, true) != EOF);
    }

    current->pde = (page_entry_t *)copy_pde();
    set_cr3(current->pde);

    iframe.gs = 0;
    iframe.ds = (USER_DATA_SEG << 3) | DPL_USER;
    iframe.es = (USER_DATA_SEG << 3) | DPL_USER;
    iframe.fs = (USER_DATA_SEG << 3) | DPL_USER;
    iframe.ss = (USER_DATA_SEG << 3) | DPL_USER;
    iframe.cs = (USER_CODE_SEG << 3) | DPL_USER;

    iframe.error = RDIX_MAGIC;
    iframe.eip = (u32)*target;

    /* flage 中 IOPL 是控制所有 IO 权限的开关
     * 只有当 CPL <= IOPL 时，任务才允许访问所有 IO 端口，否则只能根据 tss 中的 io 位图来访问io */
    iframe.eflags = (0 << 12 | 0b10 | 1 << 9);
    iframe.esp = USER_STACK_TOP;

    /* 修改内核栈指针 */
    asm volatile(
        "movl %0, %%esp\n"
        "jmp interrupt_exit\n"
        :
        :"m"(kernel_stack)
    );
}

void *user_task_create(user_target_t target, const char *name, u32 priority){

    /* target 是存放在栈中的，user_task_create 是会返回的
     * 一旦函数返回 target 内容就被释放了，因此需要额外创建一个容器存放 target */
    user_target_t *container = (user_target_t *)malloc(sizeof(user_target_t));
    /* 该进程结束时应当回收内存 */
    *container = target;

    /* kernel_to_user 任务是用户进程进行任务切换的媒介
     * kernel_to_user 的 TCB 就是用户进程的 TCB */
    task_create((task_program)kernel_to_user, (void *)container, name, priority, USER_UID);
}

/* fork 是用户进程的系统调用，使用的是中断门
 * 进入后 cpu 会自动关中断，不需要关心竞态问题 */
pid_t sys_fork(){
    TCB_t *task = (TCB_t *)running_task->owner;
    TCB_t *child = (TCB_t *)alloc_kpage(1);
    
    u32 ebp;
    asm volatile(
        "movl %%ebp, %0":"=m"(ebp)
    );

    /* 一共要向上走 ebp 和 eip 和 int0x80 传入的四个参数长度 */
    intr_frame_t *child_frame = (intr_frame_t *)(ebp + sizeof(u32) * 6 - (u32)task + (u32)child);

    /* 复制 TCB 以及内核栈 */
    memcpy((void *)child, (void *)task, PAGE_SIZE);

    child->pid = ++task_cnt;
    child->ppid = task->pid;
    child->ticks = child->priority;
    child->state = TASK_READY;

    child->vmap = malloc(sizeof(bitmap_t));
    memcpy(child->vmap, task->vmap, sizeof(bitmap_t));

    void *buf = (void *)alloc_kpage(1);
    memcpy(buf, task->vmap->start, PAGE_SIZE);
    child->vmap->start = buf;

    child->pde = (page_entry_t *)copy_pde();

    /* 子进程返回值为 0 */
    child_frame->eax = 0;
    task_stack_t *child_stack = (task_stack_t *)((u32)child_frame - sizeof(task_stack_t));

    child_stack->ebp = RDIX_MAGIC;
    child_stack->ebx = RDIX_MAGIC;
    child_stack->edi = RDIX_MAGIC;
    child_stack->esi = RDIX_MAGIC;

    child_stack->eip = interrupt_exit;

    child->stack = child_stack;

    /* 将 child 进程加入 ready 队列 */
    list_push(ready_list, new_listnode(child, child->priority));

    return child->pid;
}

/* TCB 中状态值没有修改
 * 若 task == NULL，代表阻塞当前任务
 * block 是将任务压入列表顶部 */
void block(List_t *list, ListNode_t *task){
    bool IF_stat = get_IF();
    set_IF(false);

    if (!task){
        list_push(list, running_task);
        schedule();
    }
    else{
        assert(task->container);
        remove_node(task);
        list_push(list, task);
    }

    set_IF(IF_stat);
}

/* 如果输入节点为空，那么直接弹出阻塞链表尾部节点
 * 并将其加入就绪队列 */
void unblock(ListNode_t *task){
    bool IF_stat = get_IF();
    set_IF(false);

    if (!task)
        task = list_popback(block_list);

    remove_node(task);

    list_push(ready_list, task);
    ((TCB_t*)task->owner)->state = TASK_READY;

    set_IF(IF_stat);
}

/* 调用软中断后好像 IF 位也会自动清 0 */
void task_sleep(time_t time){
    assert(!get_IF());

    ListNode_t *current = current_task();
    time_t ticks = time / JIFFY;

    if (time % JIFFY)
        ++ticks;

    current->value = jiffies + ticks;

    ((TCB_t*)current->owner)->state = TASK_SLEEPING;
    TCB_t *tmp = (TCB_t*)current->owner;

    /* 这里不许要删除节点，因为主动调用 sleep 的任务必然是当前任务，不属于其他状态链表 */
    list_insert(sleep_list, current, greater);

    schedule();
}

void weakup(){
    assert(!get_IF());
    
    ListNode_t *iter = sleep_list->end.next;
    if (iter->owner != NULL && jiffies >= iter->value){   
        remove_node(iter);
        ((TCB_t*)iter->owner)->state = TASK_READY;
        list_push(ready_list, iter);
    }
}

void task_init(){
    kernel_task_init();
}