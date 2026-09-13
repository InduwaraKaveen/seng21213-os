#include "../include/process.h"

static uint8_t process_stacks[MAX_PROCS][STACK_SIZE]
    __attribute__((aligned(16)));

pcb_t proc_table[MAX_PROCS];
int current_proc = 0;

static uint32_t next_pid = 1;

void proc_init(void)
{
    for (int i = 0; i < MAX_PROCS; i++) {
        proc_table[i].pid = 0;
        proc_table[i].state = PROC_UNUSED;
        proc_table[i].esp = 0;
        proc_table[i].stack_base = 0;
        proc_table[i].entry = NULL;
        proc_table[i].name[0] = '\0';
        proc_table[i].ticks = 0;
    }

    current_proc = 0;
    next_pid = 1;
}

static uint32_t process_stack_init(uint32_t stack_top, void (*entry)(void))
{
    uint32_t *stack = (uint32_t *)stack_top;

        /*
     * Build the stack in reverse order because the stack grows downward.
     *
     * After switching ESP to this address, irq0_stub executes:
     *
     *     popa
     *     iretd
     *
     * Therefore the final memory layout must be:
     *
     *     EDI
     *     ESI
     *     EBP
     *     ESP (ignored by popa)
     *     EBX
     *     EDX
     *     ECX
     *     EAX
     *     EIP
     *     CS
     *     EFLAGS
     */

    /* iret frame */
    *(--stack) = 0x00000202;          /* EFLAGS: IF enabled */
    *(--stack) = 0x00000008;          /* CS */
    *(--stack) = (uint32_t)entry;     /* EIP */

    /* popa frame -- pushed in reverse order */
    *(--stack) = 0;                   /* EAX */
    *(--stack) = 0;                   /* ECX */
    *(--stack) = 0;                   /* EDX */
    *(--stack) = 0;                   /* EBX */
    *(--stack) = 0;                   /* ESP (ignored by popa) */
    *(--stack) = 0;                   /* EBP */
    *(--stack) = 0;                   /* ESI */
    *(--stack) = 0;                   /* EDI */

    return (uint32_t)stack;
}

pcb_t *proc_create(const char *name, void (*entry)(void))
{
    for (int i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_UNUSED) {
            pcb_t *proc = &proc_table[i];

            proc->pid = next_pid++;
            proc->state = PROC_READY;
            proc->stack_base = (uint32_t)&process_stacks[i][0];
            proc->esp = process_stack_init(proc->stack_base + STACK_SIZE, entry);
            proc->entry = entry;
            proc->ticks = 0;

            int j = 0;
            while (name[j] != '\0' && j < 31) {
                proc->name[j] = name[j];
                j++;
            }
            proc->name[j] = '\0';

            return proc;
        }
    }

    return NULL;
}

void proc_exit(void)
{
    proc_table[current_proc].state = PROC_ZOMBIE;

    /* Scheduling will be added in the next part. */
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
