#ifndef __LIST_H__
#define __LIST_H__

#include <common/type.h>

#define INFINTY 0xffffffff

struct List;

typedef struct List_Node{
    u32 value;
    struct List_Node *next;
    struct List_Node *previous;
    void *owner;
    struct List *container;
} ListNode_t;

typedef struct List{
    u32 number_of_node;
    ListNode_t end;
} List_t;

typedef bool (*Cmp_t)(u32, u32);

List_t *new_list();
ListNode_t *new_listnode(void *owner, u32 value);
bool list_isempty(List_t *list);
void list_push(List_t *list, ListNode_t *node);
ListNode_t *list_pop(List_t *list);
void list_pushback(List_t *list, ListNode_t *node);
ListNode_t *list_popback(List_t *list);
void list_insert(List_t *list, ListNode_t *node, Cmp_t cmp);

bool less(u32, u32);
bool more(u32, u32);

#endif