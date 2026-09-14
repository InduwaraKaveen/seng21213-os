#ifndef PMM_H
#define PMM_H

#include "types.h"

#define FRAME_SIZE 4096u

uint32_t pmm_alloc_frame(void);
void     pmm_free_frame(uint32_t phys);

uint32_t pmm_free_frames(void);
uint32_t pmm_total_frames(void);

void pmm_init(void);

#endif
