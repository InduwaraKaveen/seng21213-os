#ifndef SCHEDULER_H
#define SCHEDULER_H

typedef enum {
    SCHED_PROCESS = 0,
    SCHED_THREAD
} sched_unit_type_t;

void scheduler_init(void);

/* Existing process scheduler. */
int scheduler_tick(void);

/* Stage 2 thread scheduler. */
int thread_scheduler_tick(void);

/* Current CPU execution unit. */
extern sched_unit_type_t current_unit_type;

#endif
