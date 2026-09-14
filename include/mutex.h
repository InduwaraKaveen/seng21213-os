#ifndef MUTEX_H
#define MUTEX_H

#include "types.h"

#define MUTEX_MAX_WAITERS 16

typedef struct {
    volatile uint32_t locked;
    int owner;

    int waiters[MUTEX_MAX_WAITERS];
    int wait_count;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif
