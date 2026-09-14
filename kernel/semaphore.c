#include "../include/semaphore.h"
#include "../include/thread.h"

static void semaphore_add_waiter(semaphore_t *sem, int thread_index)
{
    if (sem->wait_count >= SEM_MAX_WAITERS) {
        return;
    }

    sem->waiters[sem->wait_count++] = thread_index;
}

static int semaphore_remove_waiter(semaphore_t *sem)
{
    if (sem->wait_count == 0) {
        return -1;
    }

    int thread_index = sem->waiters[0];

    for (int i = 1; i < sem->wait_count; i++) {
        sem->waiters[i - 1] = sem->waiters[i];
    }

    sem->waiters[sem->wait_count - 1] = -1;
    sem->wait_count--;

    return thread_index;
}

void semaphore_init(semaphore_t *sem, int value)
{
    sem->value = value;
    sem->wait_count = 0;

    for (int i = 0; i < SEM_MAX_WAITERS; i++) {
        sem->waiters[i] = -1;
    }
}

void semaphore_wait(semaphore_t *sem)
{
    for (;;) {
        int acquired = 0;

        __asm__ __volatile__("cli");

        if (sem->value > 0) {
            sem->value--;
            acquired = 1;
        } else if (current_thread >= 0 &&
                   thread_table[current_thread].state != PROC_BLOCKED) {
            semaphore_add_waiter(sem, current_thread);
            thread_table[current_thread].state = PROC_BLOCKED;
        }

        __asm__ __volatile__("sti");

        if (acquired) {
            return;
        }

        /*
         * The semaphore is unavailable. Sleep until an interrupt
         * gives the scheduler an opportunity to run us again.
         */
        __asm__ __volatile__("hlt");
    }
}

void semaphore_signal(semaphore_t *sem)
{
    __asm__ __volatile__("cli");

    sem->value++;

    int waiter = semaphore_remove_waiter(sem);

    if (waiter >= 0 && waiter < MAX_THREADS &&
        thread_table[waiter].state == PROC_BLOCKED) {
        thread_table[waiter].state = PROC_READY;
    }

    __asm__ __volatile__("sti");
}
