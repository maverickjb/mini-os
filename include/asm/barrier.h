#ifndef __ASM_BARRIER_H
#define __ASM_BARRIER_H

static inline void smp_mb(void)
{
    __asm__ volatile("dmb ish" : : : "memory");
}

#endif
