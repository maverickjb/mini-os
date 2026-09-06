#ifndef __LINUX_SMP_H
#define __LINUX_SMP_H

/*
 * Software Generated Interrupt (SGI) IDs — doorbells between CPUs.
 * SGIs are IRQ numbers 0–15 in the GIC.
 */
#define IPI_RESCHEDULE  0

void gic_send_sgi(unsigned int cpu, unsigned int sgi);
void send_reschedule_ipi(unsigned int cpu);
void handle_ipi(unsigned int ipi);
void handle_reschedule_ipi(void);
void gic_secondary_init(unsigned int cpu);

#endif
