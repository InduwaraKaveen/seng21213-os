#include "../include/ramdisk.h"

#define RAMDISK_ALIGNMENT 4096u

static uint8_t ramdisk_storage[RAMDISK_SIZE]
    __attribute__((section(".ramdisk"), aligned(RAMDISK_ALIGNMENT)));

void ramdisk_init(void)
{
    for (uint32_t i = 0; i < RAMDISK_SIZE; i++) {
        ramdisk_storage[i] = 0;
    }
}

void ramdisk_read(uint32_t offset, void *buffer, uint32_t size)
{
    if (buffer == 0) {
        return;
    }

    if (offset >= RAMDISK_SIZE) {
        return;
    }

    if (size > RAMDISK_SIZE - offset) {
        size = RAMDISK_SIZE - offset;
    }

    uint8_t *dst = (uint8_t *)buffer;

    for (uint32_t i = 0; i < size; i++) {
        dst[i] = ramdisk_storage[offset + i];
    }
}

void ramdisk_write(uint32_t offset, const void *buffer, uint32_t size)
{
    if (buffer == 0) {
        return;
    }

    if (offset >= RAMDISK_SIZE) {
        return;
    }

    if (size > RAMDISK_SIZE - offset) {
        size = RAMDISK_SIZE - offset;
    }

    const uint8_t *src = (const uint8_t *)buffer;

    for (uint32_t i = 0; i < size; i++) {
        ramdisk_storage[offset + i] = src[i];
    }
}

uint8_t *ramdisk_data(void)
{
    return ramdisk_storage;
}
