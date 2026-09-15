#include "sleep.h"
#include "process.h"
#include "thread.h"
#include "interrupts.h"
#include "scheduler.h"

void sleep(uint32_t ms)
{
    if (ms == 0) {
        return;
    }

    /*
     * PIT runs at 100 Hz, so one timer tick is 10 ms.
     * Round up so sleep(1) still sleeps for at least one tick.
     */
    uint32_t ticks = ms / 10;
    if (ms % 10 != 0) {
        ticks++;
    }

    /*
     * Remember exactly which execution unit called sleep().
     * current_proc/current_thread can change while this unit
     * is descheduled.
     */
    sched_unit_type_t sleeping_type = current_unit_type;
    int sleeping_index;

    if (sleeping_type == SCHED_PROCESS) {
        if (current_proc < 0 || current_proc >= MAX_PROCS) {
            return;
        }

        sleeping_index = current_proc;

        if (proc_table[sleeping_index].state != PROC_RUNNING) {
            return;
        }

        __asm__ volatile ("cli");

        proc_table[sleeping_index].wake_tick = timer_ticks + ticks;
        proc_table[sleeping_index].state = PROC_BLOCKED;

        __asm__ volatile ("sti");
    } else {
        if (current_thread < 0 || current_thread >= MAX_THREADS) {
            return;
        }

        sleeping_index = current_thread;

        if (thread_table[sleeping_index].state != PROC_RUNNING) {
            return;
        }

        __asm__ volatile ("cli");

        thread_table[sleeping_index].wake_tick = timer_ticks + ticks;
        thread_table[sleeping_index].state = PROC_BLOCKED;

        __asm__ volatile ("sti");
    }

    /*
     * The PIT interrupt performs the actual context switch.
     * Until this unit is scheduled again, remain idle here.
     */
    for (;;) {
        __asm__ volatile ("hlt");

        if (sleeping_type == SCHED_PROCESS) {
            if (proc_table[sleeping_index].state != PROC_BLOCKED) {
                break;
            }
        } else {
            if (thread_table[sleeping_index].state != PROC_BLOCKED) {
                break;
            }
        }
    }
}

/*
 * Wake processes and threads whose sleep deadline has arrived.
 */
void sleep_tick(void)
{
    for (int i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_BLOCKED &&
            proc_table[i].wake_tick != 0 &&
            timer_ticks >= proc_table[i].wake_tick) {
            proc_table[i].wake_tick = 0;
            proc_table[i].state = PROC_READY;
        }
    }

    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state == PROC_BLOCKED &&
            thread_table[i].wake_tick != 0 &&
            timer_ticks >= thread_table[i].wake_tick) {
            thread_table[i].wake_tick = 0;
            thread_table[i].state = PROC_READY;
        }
    }
}
