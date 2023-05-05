#include <rdix/mutex.h>
#include <common/assert.h>
#include <rdix/kernel.h>
#include <rdix/task.h>
#include <common/interrupt.h>

void mutex_init(mutex_t *mutex){
    mutex->value = 1;
    mutex->waiter = new_list();
}

mutex_t *new_mutex(){
    mutex_t *mutex = (mutex_t *)malloc(sizeof(mutex_t));

    mutex_init(mutex);

    return mutex;
}

/* 上锁时返回 true */
bool test_lock(mutex_t *mutex){
    return !(mutex->value);
}

void mutex_lock(mutex_t *mutex){
    assert(mutex);

    ATOMIC_OPS(
        if (mutex->value && !mutex->waiter->number_of_node) \
            mutex->value = 0; \
        else \
            block(mutex->waiter, NULL, TASK_BLOCKED);
    );
}

void mutex_unlock(mutex_t *mutex){
    assert(mutex);

    ATOMIC_OPS(
        if (!mutex->waiter->number_of_node) \
            mutex->value = 1; \
        else{ \
            unblock(mutex->waiter->end.previous); \
        }
    );
}