#ifndef SLEEP_H
#define SLEEP_H

#include "types.h"

/*
 * Put the current process/thread to sleep for the requested
 * number of milliseconds.
 */
void sleep(uint32_t ms);

/* Called from the PIT timer interrupt on every tick. */
void sleep_tick(void);

#endif
