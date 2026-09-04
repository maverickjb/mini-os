#ifndef SMP_H
#define SMP_H

#define NR_CPUS 4

/*
 * always_inline: at -O0 GCC will not inline a plain static inline and
 * still emits an external call, which then fails to link.
 */
static inline __attribute__((always_inline)) unsigned int smp_processor_id(void)
{
    unsigned long mpidr;

    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));

    return (unsigned int)(mpidr & 0xff);
}

void smp_init(void);
int cpu_up(unsigned int cpu);
void bringup_nonboot_cpus(void);
void secondary_main(void);

#endif
