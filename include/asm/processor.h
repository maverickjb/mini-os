#ifndef __ASM_PROCESSOR_H
#define __ASM_PROCESSOR_H

static inline void cpu_relax(void)
{
    asm volatile("yield");
}

#endif /* __ASM_PROCESSOR_H */
