#ifndef BARRIER_H
#define BARRIER_H

#include "types.h"

#define BARRIER_MAX_WAITERS 16

typedef struct {
    uint32_t threshold;
    uint32_t count;
    uint32_t generation;
    int waiters[BARRIER_MAX_WAITERS];
} barrier_t;

void barrier_init(barrier_t *barrier, uint32_t threshold);
void barrier_wait(barrier_t *barrier);

#endif
