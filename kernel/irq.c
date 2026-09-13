#include "../include/interrupts.h"
#include "../include/io.h"
#include "../include/scheduler.h"
#include "../include/process.h"

volatile uint32_t timer_ticks = 0;

uint32_t *irq0_handler(uint32_t *saved_esp)
{
    timer_ticks++;

    proc_table[current_proc].esp = (uint32_t)saved_esp;

    int next = scheduler_tick();

    outb(0x20, 0x20);

    return (uint32_t *)proc_table[next].esp;
}
