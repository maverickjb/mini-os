#ifndef __ASM_BARRIER_H
#define __ASM_BARRIER_H

static inline void smp_mb(void)
{
    __asm__ volatile("dmb ish" : : : "memory");
}

static inline __attribute__((always_inline)) void
smp_store_release(volatile int *p, int v)
{
    __asm__ volatile("stlr %w1, [%0]" : : "r"(p), "r"(v) : "memory");
}

static inline __attribute__((always_inline)) int
smp_load_acquire(volatile int *p)
{
    int v;

    __asm__ volatile("ldar %w0, [%1]" : "=r"(v) : "r"(p) : "memory");
    return v;
}

#endif
