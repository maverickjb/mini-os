#ifndef SMP_H
#define SMP_H

#define NR_CPUS 4

void smp_init(void);
int cpu_up(unsigned int cpu);
void bringup_nonboot_cpus(void);
void secondary_main(void);

#endif
