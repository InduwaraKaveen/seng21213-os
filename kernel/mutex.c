#include "../include/mutex.h"
#include "../include/thread.h"

static void mutex_add_waiter(mutex_t *mutex, int thread_index)
{
    if (mutex->wait_count >= MUTEX_MAX_WAITERS) {
        return;
    }

    mutex->waiters[mutex->wait_count++] = thread_index;
}

static int mutex_remove_waiter(mutex_t *mutex)
{
    if (mutex->wait_count == 0) {
        return -1;
    }

    int thread_index = mutex->waiters[0];

    for (int i = 1; i < mutex->wait_count; i++) {
        mutex->waiters[i - 1] = mutex->waiters[i];
    }

    mutex->waiters[mutex->wait_count - 1] = -1;
    mutex->wait_count--;

    return thread_index;
}

void mutex_init(mutex_t *mutex)
{
    mutex->locked = 0;
    mutex->owner = -1;
    mutex->wait_count = 0;

    for (int i = 0; i < MUTEX_MAX_WAITERS; i++) {
        mutex->waiters[i] = -1;
    }
}

void mutex_lock(mutex_t *mutex)
{
    uint32_t old_value;

    for (;;) {
        /*
         * Disable interrupts while attempting to acquire the mutex
         * and, if necessary, entering the wait queue.
         *
         * This makes the check-and-block operation atomic with
         * respect to the timer interrupt and scheduler.
         */
        __asm__ __volatile__("cli");

        old_value = 1;

        __asm__ __volatile__(
            "xchgl %0, %1"
            : "+r"(old_value), "+m"(mutex->locked)
            :
            : "memory"
        );

        if (old_value == 0) {
            if (current_thread >= 0) {
                mutex->owner = (int)thread_table[current_thread].tid;
            } else {
                mutex->owner = -1;
            }

            __asm__ __volatile__("sti");
            return;
        }

        /*
         * The mutex is already locked.
         *
         * Put the current thread on the wait queue and mark it
         * BLOCKED before re-enabling interrupts.  Therefore a timer
         * interrupt cannot occur between discovering that the mutex
         * is busy and blocking the thread.
         */
        if (current_thread >= 0 &&
            thread_table[current_thread].state != PROC_BLOCKED) {

            mutex_add_waiter(mutex, current_thread);
            thread_table[current_thread].state = PROC_BLOCKED;
        }

        __asm__ __volatile__("sti");

        /*
         * Sleep until an interrupt gives the scheduler an opportunity
         * to run another thread.  When mutex_unlock() wakes this
         * thread, the loop retries the atomic xchg.
         */
        __asm__ __volatile__("hlt");
    }
}

void mutex_unlock(mutex_t *mutex)
{
    if (mutex->locked == 0) {
        return;
    }

    mutex->owner = -1;

    __asm__ __volatile__(
        "movl $0, %0"
        : "=m"(mutex->locked)
        :
        : "memory"
    );

    /*
     * Wake the oldest waiting thread, if any.
     */
    __asm__ __volatile__("cli");

    int waiter = mutex_remove_waiter(mutex);

    if (waiter >= 0 && waiter < MAX_THREADS &&
        thread_table[waiter].state == PROC_BLOCKED) {
        thread_table[waiter].state = PROC_READY;
    }

    __asm__ __volatile__("sti");
}
