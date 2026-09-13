#include "../include/scheduler.h"
#include "../include/process.h"

void scheduler_init(void)
{
    current_proc = 0;
}

int scheduler_tick(void)
{
    proc_table[current_proc].ticks++;

    int next = (current_proc + 1) % MAX_PROCS;
    int searched = 0;

    while (searched < MAX_PROCS) {
        if (proc_table[next].state == PROC_READY) {
            break;
        }

        next = (next + 1) % MAX_PROCS;
        searched++;
    }

    if (searched == MAX_PROCS) {
        return current_proc;
    }

    proc_table[current_proc].state = PROC_READY;
    proc_table[next].state = PROC_RUNNING;
    current_proc = next;

    return next;
}
