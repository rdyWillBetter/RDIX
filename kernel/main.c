#include <rdix/kernel.h>
#include <common/io.h>
#include <common/console.h>
#include <common/string.h>
#include <common/global.h>
#include <common/interrupt.h>
#include <common/time.h>
#include <rdix/memory.h>
#include <rdix/task.h>
#include <rdix/syscall.h>
#include <rdix/multiboot2.h>
#include <common/list.h>

/*
bool cmp(u32 a, u32 b){
    return a < b;
}

void list_test(){
    List_t *list = new_list();

    for (int i = 0; i < 10; ++i){
        ListNode_t *node = new_listnode(current_task(), i);
        list_insert(list, node, cmp);
    }

    printk("list->number_of_node = %d\n", list->number_of_node);
    BMB;
    for (ListNode_t *node = list->end.next;\
        node != &list->end;\
        node = node->next){
        printk("node->value = %d\n", node->value);
        printk("node->owner = %#p\n", node->owner);
        printk("&current_task_TCB = %#p\n", current_task());
        printk("node->container = %#p\n", node->container);
        printk("&list = %#p\n", list);
    }
}
*/

void handle_a(void){
    while(true){
        printk("a");
    }
}
void handle_b(void){
    while(true){
        printk("b");
    }
}
void handle_c(void){
    while(true){
        printk("c");
    }
}
void handle_d(void){
    while(true){
        printk("d");
    }
}

/* 当通过 rdix 自己的 loader 启动时 info 保存的是内存信息
 * 当通过 grub 启动时，info 保存的时 boot infomation */
void kernel_init(u32 magic, u32 info){
    console_init();

    if (magic == RDIX_MAGIC)
        printk("###Boot by RDIX LOADER###\n");
    else if (magic == MULTIBOOT_OS_MAGIC)
        printk("###Boot by MULTIBOOT2###\n");

    gdt_init();
    mem_pg_init(magic,info);
    task_init();
    interrupt_init();

    task_create(handle_a,"a",3,0);
    task_create(handle_b,"b",3,0);
    task_create(handle_c,"c",3,0);
    task_create(handle_d,"d",3,0);
    
}