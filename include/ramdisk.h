#ifndef RAMDISK_H
#define RAMDISK_H

#include "types.h"

#define RAMDISK_SIZE  (1024u * 1024u)
#define RAMDISK_BLOCK 512u

void ramdisk_init(void);

void ramdisk_read(uint32_t offset, void *buffer, uint32_t size);
void ramdisk_write(uint32_t offset, const void *buffer, uint32_t size);

uint8_t *ramdisk_data(void);

#endif
