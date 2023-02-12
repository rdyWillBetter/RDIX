#include <common/queue.h>
#include <rdix/kernel.h>
#include <common/assert.h>

que_char_t *new_char_que(size_t size){
    que_char_t *que = (que_char_t*)malloc(sizeof(que_char_t));

    que->head = 0;
    que->tail = 1;
    que->size = size;
    que->que = malloc(size);

    return que;
}

bool que_isempty(que_char_t *que){
    return ((que->head + 1) % que->size) == que->tail;
}

bool que_isfull(que_char_t *que){
    return que->head == que->tail;
}

void que_push(que_char_t *que, char value){
    assert(!que_isfull(que));
    que->que[que->tail] = value;
    que->tail = (que->tail + 1) % que->size;
}

char que_pop(que_char_t *que){
    assert(!que_isempty(que));
    que->head = (que->head + 1) % que->size;
    return que->que[que->head];
}