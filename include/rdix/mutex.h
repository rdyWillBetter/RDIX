#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <common/type.h>
#include <common/list.h>

typedef struct mutex_t{
    bool value;
    List_t *waiter;    
} mutex_t;

mutex_t *new_mutex();
void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
bool test_lock(mutex_t *mutex);

#endif