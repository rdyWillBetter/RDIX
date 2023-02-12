#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <common/type.h>

typedef struct que_char_t{
    u32 head;
    u32 tail;
    size_t size;
    char *que;
} que_char_t;

que_char_t *new_char_que(size_t size);
bool que_isempty(que_char_t *que);
bool que_isfull(que_char_t *que);
void que_push(que_char_t *que, char value);
char que_pop(que_char_t *que);

#endif
