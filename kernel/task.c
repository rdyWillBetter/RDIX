#include <rdix/task.h>
#include <rdix/memory.h>
#include <rdix/kernel.h>
#include <common/string.h>
#include <common/assert.h>
#include <common/list.h>

#define TASK_NUM 64

extern bitmap_t v_bit_map;
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
        "sti\n"
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

ListNode_t *task_create(task_program handle, const char *name, u32 priority, u32 uid){
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
    
    stack->edi = 0x1;
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
    tcb->pde = 0x1000; //内核页目录，修改过memory.c后要注意这里可能出问题
    tcb->vmap = &v_bit_map; //内核虚拟内存位图，同上
    tcb->magic = RDIX_MAGIC;

    /* 降序插入 */
    list_insert(ready_list, node, less);

    return node;
}

/*
static TCB_t *search_task(task_state_t state){
    TCB_t *dest_tcb = NULL;

    for (int i = 0; i < TASK_NUM; ++i){
        if (task_table[i] == NULL || task_table[i] == current_task() || task_table[i]->state != state)
            continue;

        if (dest_tcb == NULL || dest_tcb->priority < task_table[i]->priority || task_table[i]->jiffies < dest_tcb->jiffies)
            dest_tcb = task_table[i];
    }
    return dest_tcb;
}
*/

void schedule(){
    ListNode_t *next = list_pop(ready_list);
    ListNode_t *current = running_task;

    if (next == NULL)
        return;

    assert(((TCB_t *)next->owner)->magic == RDIX_MAGIC);

    ((TCB_t *)running_task->owner)->state = TASK_READY;
    ((TCB_t *)next->owner)->state = TASK_RUNNING;

    list_insert(ready_list, running_task, less);
    running_task = next;

    task_switch(current->owner, next->owner);
}
/*
void schedule(){
    //current_task 为当前任务资源地址
    TCB_t *current = current_task();
    TCB_t *next = search_task(TASK_READY);

    if (next == NULL)
        return;
    
    assert(next->magic == RDIX_MAGIC);

    current->state = TASK_READY;
    next->state = TASK_RUNNING;

    task_switch(next);
}
*/
/*
void handle_a(){
    char *name = task_name();
    while (true){
        printk("%s", name);
    }
}

void handle_b(){
    char *name = task_name();
    while (true){
        printk("%s", name);
    }
}

void handle_test(){
    char *name = task_name();
    while (true){
        printk("%s", name);
    }
}
*/
void task_init(){
    kernel_task_init();
}