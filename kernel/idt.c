#include "../include/idt.h"
#include "../include/io.h"
#include "../include/interrupts.h"

extern void irq0_stub(void);
extern void default_interrupt_stub(void);

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

static void idt_set_gate(uint8_t vector, uint32_t handler)
{
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector    = 0x08;
    idt[vector].zero        = 0;
    idt[vector].type_attr   = 0x8E;
    idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}


static void pic_remap(void)
{
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();

    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();

    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    /* Start with all hardware IRQ lines masked. */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void pic_unmask_irq(uint8_t irq)
{
    if (irq < 8) {
        uint8_t mask = inb(0x21);
        mask &= (uint8_t)~(1u << irq);
        outb(0x21, mask);
    } else if (irq < 16) {
        uint8_t mask = inb(0xA1);
        mask &= (uint8_t)~(1u << (irq - 8));
        outb(0xA1, mask);

        /* Enable the master PIC's cascade line for the slave PIC. */
        uint8_t master_mask = inb(0x21);
        master_mask &= (uint8_t)~(1u << 2);
        outb(0x21, master_mask);
    }
}

static void idt_load(void)
{
    __asm__ __volatile__(
        "lidt %0"
        :
        : "m"(idt_ptr)
    );
}

void idt_init(void)
{
    pic_remap();
    pic_unmask_irq(0);

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((uint8_t)i, (uint32_t)default_interrupt_stub);
    }

    idt_set_gate(32, (uint32_t)irq0_stub);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint32_t)&idt;

    idt_load();
}
