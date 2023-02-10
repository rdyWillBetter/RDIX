#include <common/list.h>
#include <rdix/kernel.h>
#include <common/assert.h>

List_t *new_list(){
    List_t *list = (List_t *)malloc(sizeof(List_t));

    list->number_of_node = 0;

    list->end.previous = &list->end;
    list->end.next = &list->end;
    list->end.value = INFINTY;
    list->end.owner = NULL;
    list->end.container = list;

    return list;
}

ListNode_t *new_listnode(void *owner, u32 value){
    ListNode_t *node = (ListNode_t *)malloc(sizeof(ListNode_t));

    node->value = value;
    node->next = node;
    node->previous = node;
    node->owner = owner;
    node->container = NULL;

    return node;
}

bool list_isempty(List_t *list){
    assert(list);

    return list->number_of_node == 0;
}

static void insert_before_anchor(ListNode_t *anchor, ListNode_t *node){
    assert(anchor && node);
    
    node->previous = anchor->previous;
    node->next = anchor;

    anchor->previous = node;
    node->previous->next = node;

    node->container = anchor->container;
    ++node->container->number_of_node;
}

static void insert_after_anchor(ListNode_t *anchor, ListNode_t *node){
    assert(anchor && node);

    node->next = anchor->next;
    node->previous = anchor;

    anchor->next = node;
    node->next->previous = node;

    node->container = anchor->container;
    ++node->container->number_of_node;
}

static void remove_node(ListNode_t *node){
    List_t *list = node->container;

    assert(list != NULL);

    node->next->previous = node->previous;
    node->previous->next = node->next;

    node->next = node;
    node->previous = node;
    node->container = NULL;

    --list->number_of_node;
}

void list_push(List_t *list, ListNode_t *node){
    /* 表明该节点不在任何链表中 */
    assert(node->container == NULL);

    insert_after_anchor(&list->end, node);
}

ListNode_t *list_pop(List_t *list){
    if (list_isempty(list))
        return NULL;
    
    ListNode_t *node = list->end.next;

    remove_node(node);
    return node;
}

void list_pushback(List_t *list, ListNode_t *node){
    /* 表明该节点不在任何链表中 */
    assert(node->container == NULL);

    insert_before_anchor(&list->end, node);
}

ListNode_t *list_popback(List_t *list){
    if (list_isempty(list))
        return NULL;
    
    ListNode_t *node = list->end.previous;

    remove_node(node);
    return node;
}

/* 按 value 值的大小进行插入排序 */
/* 不能保证链表插入前的顺序，如果该链表本身就是乱的，insert 后还是乱序
 * cmp 为排序函数 */
void list_insert(List_t *list, ListNode_t *node, Cmp_t cmp){
    if (list_isempty(list)){
        list_push(list, node);
        return;
    }
        
    ListNode_t *iter = NULL;
    for (iter = list->end.next; iter != &list->end; iter = iter->next){
        if (cmp(iter->value, node->value)){
            insert_before_anchor(iter, node);
            break;
        }
    }

    if (iter == &list->end)
        list_pushback(list, node);
}

bool less(u32 a, u32 b){
    return a < b;
}

bool more(u32 a, u32 b){
    return a > b;
}