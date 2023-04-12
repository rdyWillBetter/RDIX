#include <rdix/mutex.h>
#include <common/assert.h>
#include <rdix/kernel.h>
#include <rdix/task.h>
#include <common/interrupt.h>

mutex_t *new_mutex(){
    mutex_t *mutex = (mutex_t *)malloc(sizeof(mutex_t));

    mutex->value = 1;
    mutex->waiter =new_list();

    return mutex;
}

void mutex_lock(mutex_t *mutex){
    assert(mutex);

    bool IF_state = get_IF();
    set_IF(false);

    if (mutex->value && !mutex->waiter->number_of_node)
        mutex->value = 0;
    else
        block(mutex->waiter, NULL, TASK_BLOCKED);

    set_IF(IF_state);
}

void mutex_unlock(mutex_t *mutex){
    assert(mutex);

    bool IF_state = get_IF();
    set_IF(false);

    if (!mutex->waiter->number_of_node)
        mutex->value = 1;
    else{
        unblock(mutex->waiter->end.previous);
    }

    set_IF(IF_state);
}