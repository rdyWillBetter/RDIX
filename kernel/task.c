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

static TCB_t *task_table[TASK_NUM];

List_t *block_list;
List_t *sleep_list;
List_t *ready_list;

static ListNode_t *running_task;

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

    memset(task_table, 0, sizeof(task_table));

    block_list = new_list();
    sleep_list = new_list();
    ready_list = new_list();

    asm volatile(
        "movl %%esp, %0\n"
        :"=m"(stack)
        :
        :
    );

    kernel->stack = stack;
    kernel->state = TASK_RUNNING;
    kernel->priority = 3;
    kernel->ticks = kernel->priority;
    kernel->jiffies = 0;
    strcpy((char *)kernel->name, "kernel");
    kernel->uid = 0;
    kernel->pde = 0x1000; //内核页目录，修改过memory.c后要注意这里可能出问题
    kernel->vmap = &v_bit_map; //内核虚拟内存位图，同上
    kernel->magic = RDIX_MAGIC;

    task_table[0] = kernel; //将内核加入任务队列
    /* 开始执行 */
    running_task = new_listnode(kernel, kernel->priority);
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

    for (int i = 0; i < TASK_NUM; ++i){
        if (task_table[i] != NULL)
            continue;
        else{
            task_table[i] = tcb;
            break;
        }
        if (i == TASK_NUM - 1)
            PANIC("Can not create more task!\n");
    }
    
    /* edi 为指向传入参数结构体的指针 */
    stack->edi = (u32)param;
    stack->esi = 0x2;
    stack->ebx = 0x3;
    stack->ebp = 0x4;
    stack->eip = handle;

    tcb->stack = stack;
    tcb->state = TASK_READY;
    tcb->priority = priority;
    tcb->ticks = tcb->priority;
    tcb->jiffies = 0;
    strcpy((char *)tcb->name, name);
    tcb->uid = uid;
    tcb->pde = (page_entry_t *)0x1000; //内核页目录，修改过memory.c后要注意这里可能出问题
    tcb->vmap = &v_bit_map; //内核虚拟内存位图，同上
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
    /* target 为用户程序入口地址 */
    user_target_t *target;
    intr_frame_t iframe;
    void *kernel_stack = &iframe;
    TCB_t *current = (TCB_t *)current_task()->owner;

    /* 参数指针保存在 edi 中 */
    asm volatile(
        "movl %%edi,%0\n"
        :"=m"(target)
    );

    /* 用户进程的虚拟内存空间位图以及页表待初始化 */
    
    current->vmap = (bitmap_t *)malloc(sizeof(bitmap_t));

    /* 用户进程的虚拟内存位图为 4KB，所管理的内存起始地址为 8M
     * 用户程序一共可使用的内存为 128M，也就是 8M 到 138M */
    bitmap_init(current->vmap, alloc_kpage(1), PAGE_SIZE, PAGE_IDX(KERNEL_MEMERY_SIZE));

    /* 分配栈空间 */
    for (page_idx_t i = PAGE_IDX(USER_STACK_BOTTOM); i < PAGE_IDX(USER_STACK_TOP); ++i){
        assert(bitmap_set(current->vmap, i, true) != EOF);
    }

    current->pde = copy_pde();
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

    task_create((task_program)kernel_to_user, (void *)container, name, priority, USER_UID);
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