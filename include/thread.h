#ifndef THREAD_H
#define THREAD_H

#include "types.h"
#include "process.h"

#define MAX_THREADS 16
#define THREAD_STACK_SIZE STACK_SIZE

typedef struct {
    uint32_t tid;
    uint32_t pid;
    uint32_t esp;
    uint32_t stack_base;
    proc_state_t state;
    void (*entry)(void);
    char name[24];
    uint32_t ticks;
} tcb_t;

void thread_init(void);

tcb_t *thread_create(uint32_t pid, const char *name, void (*fn)(void));

void thread_exit(void);

extern tcb_t thread_table[MAX_THREADS];
extern int current_thread;

#endif
