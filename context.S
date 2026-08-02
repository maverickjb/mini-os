/*
 * Task context switch — Linux-style switch_to on struct cpu_context.
 */

#include <asm/asm-offsets.h>

.section .text
.global switch_to
.global task_trampoline
.global finish_eret

/*
 * Set SP_EL1 to current task kernel stack top when returning to EL0.
 * Uses only x9–x11; caller must have restored x0/x30 already.
 */
.macro prepare_kstack_el0
    mrs     x11, spsr_el1
    and     x11, x11, #0xf
    cbnz    x11, 999f
    adr     x10, cpu_current_export
    ldr     x10, [x10]
    cbz     x10, 999f
    ldr     x9, [x10, #TASK_stack]
    cbz     x9, 999f
    add     x9, x9, #INIT_STACK_SIZE
    mov     sp, x9
999:
.endm

/*
 * void switch_to(struct task_struct *prev, struct task_struct *next)
 */
switch_to:
    /*
     * Save prev kernel context
     */
    stp     x19, x20, [x0, #CTX_x19_x20]
    stp     x21, x22, [x0, #CTX_x21_x22]
    stp     x23, x24, [x0, #CTX_x23_x24]
    stp     x25, x26, [x0, #CTX_x25_x26]
    stp     x27, x28, [x0, #CTX_x27_x28]
    stp     x29, x30, [x0, #CTX_fp_pc]
    mov     x9, sp
    str     x9, [x0, #CTX_sp]

    ldp     x19, x20, [x1, #CTX_x19_x20]
    ldp     x21, x22, [x1, #CTX_x21_x22]
    ldp     x23, x24, [x1, #CTX_x23_x24]
    ldp     x25, x26, [x1, #CTX_x25_x26]
    ldp     x27, x28, [x1, #CTX_x27_x28]
    ldp     x29, x30, [x1, #CTX_fp_pc]
    ldr     x9, [x1, #CTX_sp]
    mov     sp, x9

    ret

task_trampoline:
    mov     x0, x19
    blr     x20
    mov     x0, xzr
    bl      schedule

/*
 * void finish_eret(struct pt_regs *regs) — restore frame and return to EL0.
 */
finish_eret:
    mov     sp, x0

    ldp     x21, x22, [sp, #248]
    msr     elr_el1, x21
    msr     spsr_el1, x22

    ldr     x30, [sp, #240]
    ldp     x28, x29, [sp, #224]
    ldp     x26, x27, [sp, #208]
    ldp     x24, x25, [sp, #192]
    ldp     x22, x23, [sp, #176]
    ldp     x20, x21, [sp, #160]
    ldp     x18, x19, [sp, #144]
    ldp     x16, x17, [sp, #128]
    ldp     x14, x15, [sp, #112]
    ldp     x12, x13, [sp, #96]
    ldp     x10, x11, [sp, #80]
    ldp     x8, x9, [sp, #64]
    ldp     x6, x7, [sp, #48]
    ldp     x4, x5, [sp, #32]
    ldp     x2, x3, [sp, #16]
    ldp     x0, x1, [sp, #0]
    add     sp, sp, #272
    prepare_kstack_el0
    eret

.global prepare_return_to_el0
prepare_return_to_el0:
    prepare_kstack_el0
    ret
