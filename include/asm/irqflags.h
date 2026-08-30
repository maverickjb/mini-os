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

static inline unsigned long local_irq_save(void)
{
    unsigned long flags;

    __asm__ volatile("mrs %0, daif" : "=r"(flags));
    __asm__ volatile("msr DAIFSet, #2" : : : "memory");
    return flags;
}

static inline void local_irq_restore(unsigned long flags)
{
    __asm__ volatile("msr daif, %0" : : "r"(flags) : "memory");
}

#endif /* __ASM_IRQFLAGS_H */
