#include "../include/pit.h"
#include "../include/io.h"

void pit_init(void)
{
    uint32_t divisor = PIT_FREQUENCY / PIT_HZ;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}
