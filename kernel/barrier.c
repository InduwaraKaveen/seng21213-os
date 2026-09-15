#include "../include/barrier.h"
#include "../include/thread.h"

void barrier_init(barrier_t *barrier, uint32_t threshold)
{
    barrier->threshold = threshold;
    barrier->count = 0;
    barrier->generation = 0;

    for (int i = 0; i < BARRIER_MAX_WAITERS; i++) {
        barrier->waiters[i] = -1;
    }
}

void barrier_wait(barrier_t *barrier)
{
    if (barrier == 0 || barrier->threshold == 0 ||
        barrier->threshold > BARRIER_MAX_WAITERS) {
        return;
    }

    if (current_thread < 0 || current_thread >= MAX_THREADS) {
        return;
    }

    int thread_index = current_thread;

    __asm__ __volatile__("cli");

    uint32_t generation = barrier->generation;

    if (barrier->count >= barrier->threshold) {
        __asm__ __volatile__("sti");
        return;
    }

    barrier->waiters[barrier->count] = thread_index;
    barrier->count++;

    if (barrier->count == barrier->threshold) {
        /*
         * Last arriving thread releases the entire generation.
         */
        for (uint32_t i = 0; i < barrier->count; i++) {
            int waiter = barrier->waiters[i];

            if (waiter >= 0 && waiter < MAX_THREADS &&
                thread_table[waiter].state == PROC_BLOCKED) {
                thread_table[waiter].state = PROC_READY;
            }

            barrier->waiters[i] = -1;
        }

        barrier->count = 0;
        barrier->generation++;

        /*
         * The last thread itself is still RUNNING.
         */
        __asm__ __volatile__("sti");
        return;
    }

    /*
     * Wait for the remaining threads.
     */
    thread_table[thread_index].state = PROC_BLOCKED;

    __asm__ __volatile__("sti");

    while (barrier->generation == generation) {
        __asm__ __volatile__("hlt");
    }
}
