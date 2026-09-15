#ifndef RWLOCK_H
#define RWLOCK_H

#include "types.h"

#define RWLOCK_MAX_WAITERS 16

#define RWLOCK_READER 0
#define RWLOCK_WRITER 1

typedef struct {
    volatile uint32_t readers;
    volatile uint32_t writer;
    uint32_t waiting_writers;

    int waiters[RWLOCK_MAX_WAITERS];
    int waiter_types[RWLOCK_MAX_WAITERS];
    int wait_count;
} rwlock_t;

void rwlock_init(rwlock_t *lock);
void rwlock_read_lock(rwlock_t *lock);
void rwlock_read_unlock(rwlock_t *lock);
void rwlock_write_lock(rwlock_t *lock);
void rwlock_write_unlock(rwlock_t *lock);

#endif
