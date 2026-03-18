#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

#include "kernel.h"

/* ── x86_64 ticket spinlock ───────────────────────────────────────────────── */

typedef struct {
    volatile u32 next_ticket;   /* next ticket to hand out */
    volatile u32 now_serving;   /* ticket currently holding the lock */
} spinlock_t;

#define SPINLOCK_INIT  { .next_ticket = 0, .now_serving = 0 }

/* ── API ──────────────────────────────────────────────────────────────────── */

static inline void spin_lock_init(spinlock_t *lock) {
    lock->next_ticket  = 0;
    lock->now_serving  = 0;
}

static inline void spin_lock(spinlock_t *lock) {
    u32 my_ticket = __atomic_fetch_add(&lock->next_ticket, 1, __ATOMIC_SEQ_CST);
    while (__atomic_load_n(&lock->now_serving, __ATOMIC_ACQUIRE) != my_ticket)
        __asm__ volatile("pause");
}

static inline void spin_unlock(spinlock_t *lock) {
    __atomic_fetch_add(&lock->now_serving, 1, __ATOMIC_RELEASE);
}

static inline bool spin_trylock(spinlock_t *lock) {
    u32 serving = __atomic_load_n(&lock->now_serving, __ATOMIC_ACQUIRE);
    u32 next    = serving;
    return __atomic_compare_exchange_n(
        &lock->next_ticket, &next, serving + 1,
        0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
}

/* ── IRQ-safe variants ────────────────────────────────────────────────────── */

static inline u64 spin_lock_irqsave(spinlock_t *lock) {
    u64 flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags));
    spin_lock(lock);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t *lock, u64 flags) {
    spin_unlock(lock);
    __asm__ volatile("push %0; popfq" :: "r"(flags) : "memory", "cc");
}

#endif /* KERNEL_SPINLOCK_H */
