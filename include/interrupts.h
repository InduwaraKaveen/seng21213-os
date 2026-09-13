#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"

uint32_t *irq0_handler(uint32_t *saved_esp);
extern volatile uint32_t timer_ticks;

#endif
