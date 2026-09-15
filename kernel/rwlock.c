#include "../include/rwlock.h"
#include "../include/thread.h"

static int rwlock_add_waiter(rwlock_t *lock, int thread_index, int type)
{
    if (lock->wait_count >= RWLOCK_MAX_WAITERS) {
        return 0;
    }

    lock->waiters[lock->wait_count] = thread_index;
    lock->waiter_types[lock->wait_count] = type;
    lock->wait_count++;

    return 1;
}

static int rwlock_remove_waiter(rwlock_t *lock, int index)
{
    if (index < 0 || index >= lock->wait_count) {
        return -1;
    }

    int thread_index = lock->waiters[index];

    for (int i = index + 1; i < lock->wait_count; i++) {
        lock->waiters[i - 1] = lock->waiters[i];
        lock->waiter_types[i - 1] = lock->waiter_types[i];
    }

    lock->waiters[lock->wait_count - 1] = -1;
    lock->waiter_types[lock->wait_count - 1] = -1;
    lock->wait_count--;

    return thread_index;
}

static void rwlock_wake_waiters(rwlock_t *lock)
{
    if (lock->writer != 0 || lock->readers != 0 ||
        lock->wait_count == 0) {
        return;
    }

    /*
     * Wake the oldest waiter. A writer gets exclusive ownership
     * when it is scheduled; readers can acquire together when
     * there are no waiting writers.
     */
    int waiter_type = lock->waiter_types[0];
    int waiter = rwlock_remove_waiter(lock, 0);

    if (waiter >= 0 && waiter < MAX_THREADS &&
        thread_table[waiter].state == PROC_BLOCKED) {
        thread_table[waiter].state = PROC_READY;
    }

    if (waiter_type == RWLOCK_WRITER &&
        lock->waiting_writers > 0) {
        lock->waiting_writers--;
    }
}

void rwlock_init(rwlock_t *lock)
{
    lock->readers = 0;
    lock->writer = 0;
    lock->waiting_writers = 0;
    lock->wait_count = 0;

    for (int i = 0; i < RWLOCK_MAX_WAITERS; i++) {
        lock->waiters[i] = -1;
        lock->waiter_types[i] = -1;
    }
}

void rwlock_read_lock(rwlock_t *lock)
{
    for (;;) {
        int acquired = 0;

        __asm__ __volatile__("cli");

        if (lock->writer == 0 && lock->waiting_writers == 0) {
            lock->readers++;
            acquired = 1;
        } else if (current_thread >= 0 &&
                   thread_table[current_thread].state != PROC_BLOCKED) {
            if (rwlock_add_waiter(lock, current_thread, RWLOCK_READER)) {
                thread_table[current_thread].state = PROC_BLOCKED;
            }
        }

        __asm__ __volatile__("sti");

        if (acquired) {
            return;
        }

        __asm__ __volatile__("hlt");
    }
}

void rwlock_read_unlock(rwlock_t *lock)
{
    __asm__ __volatile__("cli");

    if (lock->readers > 0) {
        lock->readers--;
    }

    if (lock->readers == 0) {
        rwlock_wake_waiters(lock);
    }

    __asm__ __volatile__("sti");
}

void rwlock_write_lock(rwlock_t *lock)
{
    for (;;) {
        int acquired = 0;

        __asm__ __volatile__("cli");

        if (lock->writer == 0 && lock->readers == 0) {
            lock->writer = 1;
            acquired = 1;
        } else if (current_thread >= 0 &&
                   thread_table[current_thread].state != PROC_BLOCKED) {
            if (rwlock_add_waiter(lock, current_thread, RWLOCK_WRITER)) {
                lock->waiting_writers++;
                thread_table[current_thread].state = PROC_BLOCKED;
            }
        }

        __asm__ __volatile__("sti");

        if (acquired) {
            return;
        }

        __asm__ __volatile__("hlt");
    }
}

void rwlock_write_unlock(rwlock_t *lock)
{
    __asm__ __volatile__("cli");

    lock->writer = 0;
    rwlock_wake_waiters(lock);

    __asm__ __volatile__("sti");
}
