#include "../include/interrupts.h"
#include "../include/io.h"
#include "../include/scheduler.h"
#include "../include/sleep.h"
#include "../include/process.h"
#include "../include/thread.h"

volatile uint32_t timer_ticks = 0;

uint32_t *irq0_handler(uint32_t *saved_esp)
{
    timer_ticks++;
    sleep_tick();

    /*
     * Save the stack of whichever execution unit is currently running.
     */
    if (current_unit_type == SCHED_PROCESS) {
        proc_table[current_proc].esp = (uint32_t)saved_esp;
    } else {
        thread_table[current_thread].esp = (uint32_t)saved_esp;
    }

    /*
     * Select the next READY process or thread.
     */
    int next = scheduler_tick();

    /*
     * Send End Of Interrupt to the PIC before switching stacks.
     */
    outb(0x20, 0x20);

    /*
     * scheduler_tick() returns:
     *
     *   0 .. MAX_PROCS - 1
     *       process index
     *
     *   MAX_PROCS .. MAX_PROCS + MAX_THREADS - 1
     *       thread index + MAX_PROCS
     */
    if (current_unit_type == SCHED_PROCESS) {
        return (uint32_t *)proc_table[next].esp;
    }

    return (uint32_t *)thread_table[next - MAX_PROCS].esp;
}
