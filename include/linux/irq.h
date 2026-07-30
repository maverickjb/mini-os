#ifndef __LINUX_IRQ_H
#define __LINUX_IRQ_H

#define IRQ_TIMER   30  /* CNTPNS IRQ: EL1 physical timer (PPI) */

#include <linux/irq.h>
#include <asm/ptrace.h>

void init_IRQ(void);
void handle_arch_irq(struct pt_regs *regs);
void irq_enable(void);
void irq_disable(void);

#endif
