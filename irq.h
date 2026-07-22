#ifndef IRQ_H
#define IRQ_H

#define IRQ_TIMER   30  /* CNTPNS IRQ: EL1 physical timer (PPI) */

void init_IRQ(void);
void handle_arch_irq(void);
void irq_enable(void);
void irq_disable(void);

#endif
