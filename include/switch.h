#ifndef SWITCH_H
#define SWITCH_H

#include "types.h"

void context_switch(uint32_t *old_esp, uint32_t new_esp);

#endif
