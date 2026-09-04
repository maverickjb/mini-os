#ifndef SMP_H
#define SMP_H

#define NR_CPUS 4

extern unsigned int nr_cpus;

unsigned int smp_processor_id(void);

void smp_init(void);
int cpu_up(unsigned int cpu);
void bringup_nonboot_cpus(void);
void secondary_main(void);

#endif
