#ifndef __ASM_ASM_OFFSETS_H
#define __ASM_ASM_OFFSETS_H

#define INIT_STACK_SIZE   4096
#define PT_REGS_SIZE      272
#define SYNC_C_STACK      1024

/*
 * struct task_struct — offsets for assembly (must match linux/sched.h).
 */
#define TASK_stack        136

/*
 * struct task_struct::ctx — must match struct cpu_context in linux/sched.h.
 */
#define TASK_CTX          16
#define CTX_x19_x20       (TASK_CTX + 0)
#define CTX_x21_x22       (TASK_CTX + 16)
#define CTX_x23_x24       (TASK_CTX + 32)
#define CTX_x25_x26       (TASK_CTX + 48)
#define CTX_x27_x28       (TASK_CTX + 64)
#define CTX_fp_pc         (TASK_CTX + 80)
#define CTX_sp            (TASK_CTX + 96)

#endif /* __ASM_ASM_OFFSETS_H */
