#ifndef SMP_H
#define SMP_H

#define NR_CPUS 4

struct task_struct;

struct cpu {
    unsigned int id;
    struct rq rq;

    struct task_struct *idle;
    /* Named curr — `current` clashes with the current-task macro. */
    struct task_struct *curr;
};

extern struct cpu cpu_data[NR_CPUS];

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

/*
 * TPIDR_EL1 holds &cpu_data[this_cpu] for fast per-CPU access from C and asm.
 * Distinct from TPIDR_EL0 (userspace TLS).
 */
static inline __attribute__((always_inline)) struct cpu *this_cpu_ptr(void)
{
    unsigned long ptr;

    __asm__ volatile("mrs %0, tpidr_el1" : "=r"(ptr));
    return (struct cpu *)ptr;
}

static inline __attribute__((always_inline)) void set_cpu_local(struct cpu *cpu)
{
    __asm__ volatile("msr tpidr_el1, %0" : : "r"(cpu) : "memory");
    __asm__ volatile("isb");
}

void smp_init(void);
int cpu_up(unsigned int cpu);
void bringup_nonboot_cpus(void);
void secondary_main(void);

#endif
