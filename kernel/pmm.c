#include "../include/pmm.h"

/*
 * Stage 3 Physical Memory Manager
 *
 * QEMU is configured for 32 MiB of RAM in the Makefile.
 * 32 MiB / 4 KiB = 8192 physical frames.
 *
 * One bitmap bit represents one frame:
 *   8192 bits = 256 uint32_t = 1024 bytes.
 */

#define PMM_MEMORY_SIZE   (32u * 1024u * 1024u)
#define PMM_MAX_FRAMES    (PMM_MEMORY_SIZE / FRAME_SIZE)
#define PMM_BITMAP_WORDS  ((PMM_MAX_FRAMES + 31u) / 32u)

/*
 * Defined by linker.ld immediately after .bss.
 * This marks the first byte after the kernel image/data.
 */
extern char kernel_end;

typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
} e820_entry_t;

static uint32_t bitmap[PMM_BITMAP_WORDS];

static uint32_t total_frames;
static uint32_t free_frame_count;

static inline void bitmap_set(uint32_t frame)
{
    bitmap[frame / 32u] |= (1u << (frame % 32u));
}

static inline void bitmap_clear(uint32_t frame)
{
    bitmap[frame / 32u] &= ~(1u << (frame % 32u));
}

static inline int bitmap_test(uint32_t frame)
{
    return (bitmap[frame / 32u] >> (frame % 32u)) & 1u;
}

static uint32_t align_up_frame(uint32_t address)
{
    return (address + FRAME_SIZE - 1u) & ~(FRAME_SIZE - 1u);
}

void pmm_init(void)
{
    /*
     * Begin with every frame marked used.
     * Only E820 type-1 memory will subsequently be released.
     */
    for (uint32_t i = 0; i < PMM_BITMAP_WORDS; i++) {
        bitmap[i] = 0xFFFFFFFFu;
    }

    total_frames = PMM_MAX_FRAMES;
    free_frame_count = 0;

    uint16_t count = *(volatile uint16_t *)0x8000;
    e820_entry_t *map = (e820_entry_t *)0x8004;

    /*
     * The first 1 MiB is reserved. The kernel itself is also reserved.
     */
    uint32_t kernel_limit = (uint32_t)&kernel_end;
    if (kernel_limit < 0x00100000u) {
        kernel_limit = 0x00100000u;
    }
    kernel_limit = align_up_frame(kernel_limit);

    for (uint16_t i = 0; i < count; i++) {
        if (map[i].type != 1u) {
            continue;
        }

        uint64_t region_start64 = map[i].base;
        uint64_t region_end64 = map[i].base + map[i].length;

        /*
         * PMM is intentionally limited to the 32 MiB configured by QEMU.
         */
        if (region_start64 >= PMM_MEMORY_SIZE) {
            continue;
        }

        if (region_end64 > PMM_MEMORY_SIZE) {
            region_end64 = PMM_MEMORY_SIZE;
        }

        uint32_t start = (uint32_t)region_start64;
        uint32_t end = (uint32_t)region_end64;

        if (start < 0x00100000u) {
            start = 0x00100000u;
        }

        if (start < kernel_limit) {
            start = kernel_limit;
        }

        start = align_up_frame(start);

        /*
         * Only complete 4 KiB frames inside the usable E820 region
         * are released.
         */
        end &= ~(FRAME_SIZE - 1u);

        for (uint32_t address = start;
             address < end;
             address += FRAME_SIZE) {

            uint32_t frame = address / FRAME_SIZE;

            if (frame >= total_frames) {
                break;
            }

            /*
             * Avoid double-counting if E820 entries overlap.
             */
            if (bitmap_test(frame)) {
                bitmap_clear(frame);
                free_frame_count++;
            }
        }
    }
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t frame = 0; frame < total_frames; frame++) {
        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            free_frame_count--;
            return frame * FRAME_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(uint32_t phys)
{
    if (phys == 0 || (phys % FRAME_SIZE) != 0) {
        return;
    }

    uint32_t frame = phys / FRAME_SIZE;

    if (frame >= total_frames) {
        return;
    }

    /*
     * Only count the frame as free if it was actually allocated.
     */
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        free_frame_count++;
    }
}

uint32_t pmm_free_frames(void)
{
    return free_frame_count;
}

uint32_t pmm_total_frames(void)
{
    return total_frames;
}
