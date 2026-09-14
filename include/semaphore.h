#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "types.h"

#define SEM_MAX_WAITERS 16

typedef struct {
    volatile int value;
    int waiters[SEM_MAX_WAITERS];
    int wait_count;
} semaphore_t;

void semaphore_init(semaphore_t *sem, int value);
void semaphore_wait(semaphore_t *sem);
void semaphore_signal(semaphore_t *sem);

#endif
