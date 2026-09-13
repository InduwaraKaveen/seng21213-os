[BITS 32]

global irq0_stub
global default_interrupt_stub

extern irq0_handler

irq0_stub:
    pusha

    push esp
    call irq0_handler
    add esp, 4

    mov esp, eax

    popa
    iretd

default_interrupt_stub:
    cli
.hang:
    hlt
    jmp .hang
