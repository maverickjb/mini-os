#ifndef _LINUX_SPINLOCK_H
#define _LINUX_SPINLOCK_H

#include <asm/irqflags.h>

typedef struct {
    volatile unsigned int lock;
} spinlock_t;

#define SPINLOCK_INIT           { .lock = 0 }
#define SPINLOCK_INITIALIZER    SPINLOCK_INIT
#define DEFINE_SPINLOCK(x)      spinlock_t x = SPINLOCK_INITIALIZER

static inline void spin_lock_init(spinlock_t *lock)
{
    lock->lock = 0;
}

static inline int spin_is_locked(spinlock_t *lock)
{
    return lock->lock != 0;
}

static inline void spin_lock(spinlock_t *lock)
{
    unsigned int value;
    unsigned int status;

    asm volatile(
        "1:\n"
        "    ldaxr   %w0, [%2]\n"
        "    cbnz    %w0, 1b\n"
        "    stxr    %w1, %w3, [%2]\n"
        "    cbnz    %w1, 1b\n"
        : "=&r"(value), "=&r"(status)
        : "r"(&lock->lock), "r"(1U)
        : "memory");
}

static inline int spin_trylock(spinlock_t *lock)
{
    unsigned int value;
    unsigned int status;

    asm volatile(
        "    ldaxr   %w0, [%2]\n"
        "    cbnz    %w0, 2f\n"
        "    stxr    %w1, %w3, [%2]\n"
        "    cbnz    %w1, 2f\n"
        "    mov     %w0, #1\n"
        "    b       3f\n"
        "2:\n"
        "    mov     %w0, #0\n"
        "3:"
        : "=&r"(value), "=&r"(status)
        : "r"(&lock->lock), "r"(1U)
        : "memory");
    return (int)value;
}

static inline void spin_unlock(spinlock_t *lock)
{
    asm volatile(
        "stlr    wzr, [%0]"
        :
        : "r"(&lock->lock)
        : "memory");
}

static inline unsigned long spin_lock_irqsave(spinlock_t *lock)
{
    unsigned long flags;

    flags = local_irq_save();
    spin_lock(lock);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
    spin_unlock(lock);
    local_irq_restore(flags);
}

static inline void spin_lock_irq(spinlock_t *lock)
{
    local_irq_disable();
    spin_lock(lock);
}

static inline void spin_unlock_irq(spinlock_t *lock)
{
    spin_unlock(lock);
    local_irq_enable();
}

#define spin_lock_irqsave(lock, flags) \
    do { (flags) = spin_lock_irqsave(lock); } while (0)

#endif /* _LINUX_SPINLOCK_H */
