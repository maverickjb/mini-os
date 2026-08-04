#ifndef __ASM_IRQFLAGS_H
#define __ASM_IRQFLAGS_H

static inline void local_irq_enable(void)
{
    __asm__ volatile("msr DAIFClr, #2" : : : "memory");
}

static inline void local_irq_disable(void)
{
    __asm__ volatile("msr DAIFSet, #2" : : : "memory");
}

#endif /* __ASM_IRQFLAGS_H */
