#ifndef __LINUX_IRQ_H
#define __LINUX_IRQ_H

#define IRQ_TIMER   30  /* CNTPNS IRQ: EL1 physical timer (PPI) */
#define IRQ_UART    33  /* PL011 UART0 SPI on QEMU virt */

#include <asm/ptrace.h>

void init_IRQ(void);
void irq_enable(unsigned int irq);
void handle_arch_irq(struct pt_regs *regs);
void irq_exit(struct pt_regs *regs);

#endif
