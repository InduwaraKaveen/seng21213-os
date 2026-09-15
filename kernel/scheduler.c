#include "../include/scheduler.h"
#include "../include/process.h"
#include "../include/thread.h"

sched_unit_type_t current_unit_type = SCHED_PROCESS;

/*
 * Return values from scheduler_tick():
 *
 *   0 .. MAX_PROCS - 1
 *       process index
 *
 *   MAX_PROCS .. MAX_PROCS + MAX_THREADS - 1
 *       thread index + MAX_PROCS
 */
void scheduler_init(void)
{
    current_proc = 0;
    current_unit_type = SCHED_PROCESS;
}

int scheduler_tick(void)
{
    int current_index;

    /*
     * Mark the currently running unit as READY so that it can
     * participate in the next round-robin cycle.
     */
    if (current_unit_type == SCHED_PROCESS) {
        proc_table[current_proc].ticks++;

        /*
         * Only a running unit should become READY when its
         * time slice ends.  A BLOCKED unit must remain BLOCKED.
         */
        if (proc_table[current_proc].state == PROC_RUNNING) {
            proc_table[current_proc].state = PROC_READY;
        }

        current_index = current_proc;
    } else {
        thread_table[current_thread].ticks++;

        /*
         * Only a running thread should become READY when its
         * time slice ends.
         * BLOCKED threads must remain BLOCKED until a mutex/semaphore
         * wakes them.
         */
        if (thread_table[current_thread].state == PROC_RUNNING) {
            thread_table[current_thread].state = PROC_READY;
        }

        current_index = MAX_PROCS + current_thread;
    }

    /*
     * Search all processes and threads in round-robin order.
     *
     * Process indices:
     *     0 .. MAX_PROCS - 1
     *
     * Thread indices:
     *     MAX_PROCS .. MAX_PROCS + MAX_THREADS - 1
     */
    int total_units = MAX_PROCS + MAX_THREADS;
    int next = (current_index + 1) % total_units;

    for (int searched = 0; searched < total_units; searched++) {

        if (next < MAX_PROCS) {
            if (proc_table[next].state == PROC_READY) {
                proc_table[next].state = PROC_RUNNING;
                current_proc = next;
                current_unit_type = SCHED_PROCESS;
                return next;
            }
        } else {
            int thread_index = next - MAX_PROCS;

            if (thread_table[thread_index].state == PROC_READY) {
                thread_table[thread_index].state = PROC_RUNNING;
                current_thread = thread_index;
                current_unit_type = SCHED_THREAD;
                return MAX_PROCS + thread_index;
            }
        }

        next = (next + 1) % total_units;
    }

    /*
     * No READY unit exists.
     *
     * A blocked unit may temporarily remain the execution context
     * while it executes its HLT-based sleep wait.  Do not turn it
     * back into RUNNING here; sleep_tick() will make it READY when
     * its wake deadline arrives.
     */
    if (current_unit_type == SCHED_PROCESS) {
        if (proc_table[current_proc].state != PROC_BLOCKED) {
            proc_table[current_proc].state = PROC_RUNNING;
        }
        return current_proc;
    }

    if (thread_table[current_thread].state != PROC_BLOCKED) {
        thread_table[current_thread].state = PROC_RUNNING;
    }
    return MAX_PROCS + current_thread;
}

/*
 * Kept as a separate function for Stage 2 compatibility.
 *
 * The unified scheduler above now performs thread scheduling as
 * part of the normal timer-driven scheduling cycle.
 */
int thread_scheduler_tick(void)
{
    if (current_thread < 0 || current_thread >= MAX_THREADS) {
        return -1;
    }

    thread_table[current_thread].ticks++;

    int next = (current_thread + 1) % MAX_THREADS;
    int searched = 0;

    while (searched < MAX_THREADS) {
        if (thread_table[next].state == PROC_READY) {
            break;
        }

        next = (next + 1) % MAX_THREADS;
        searched++;
    }

    if (searched == MAX_THREADS) {
        return current_thread;
    }

    thread_table[current_thread].state = PROC_READY;
    thread_table[next].state = PROC_RUNNING;
    current_thread = next;

    return next;
}
