#include "../include/thread.h"

static uint8_t thread_stacks[MAX_THREADS][THREAD_STACK_SIZE]
    __attribute__((aligned(16)));

tcb_t thread_table[MAX_THREADS];
int current_thread = -1;

static uint32_t next_tid = 1;

static uint32_t thread_stack_init(uint32_t stack_top, void (*entry)(void))
{
    uint32_t *stack = (uint32_t *)stack_top;

    /*
     * Build the same initial interrupt-return frame used by processes.
     *
     * When this thread is eventually selected by the scheduler:
     *
     *     popa
     *     iretd
     *
     * will restore the registers and begin execution at entry().
     */

    /* iret frame */
    *(--stack) = 0x00000202;          /* EFLAGS: IF enabled */
    *(--stack) = 0x00000008;          /* CS */
    *(--stack) = (uint32_t)entry;     /* EIP */

    /* popa frame */
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

void thread_init(void)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_table[i].tid = 0;
        thread_table[i].pid = 0;
        thread_table[i].esp = 0;
        thread_table[i].stack_base = 0;
        thread_table[i].state = PROC_UNUSED;
        thread_table[i].entry = NULL;
        thread_table[i].name[0] = '\0';
        thread_table[i].ticks = 0;
    }

    current_thread = -1;
    next_tid = 1;
}

tcb_t *thread_create(uint32_t pid, const char *name, void (*fn)(void))
{
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state == PROC_UNUSED) {
            tcb_t *thread = &thread_table[i];

            thread->tid = next_tid++;
            thread->pid = pid;
            thread->state = PROC_READY;
            thread->stack_base = (uint32_t)&thread_stacks[i][0];

            if (current_thread == -1) {
                current_thread = i;
            }

            thread->esp = thread_stack_init(
                thread->stack_base + THREAD_STACK_SIZE,
                fn
            );
            thread->entry = fn;
            thread->ticks = 0;

            int j = 0;
            while (name[j] != '\0' && j < 23) {
                thread->name[j] = name[j];
                j++;
            }
            thread->name[j] = '\0';

            return thread;
        }
    }

    return NULL;
}

void thread_exit(void)
{
    if (current_thread >= 0 &&
        current_thread < MAX_THREADS) {

        thread_table[current_thread].state = PROC_ZOMBIE;
    }

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
