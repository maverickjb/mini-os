/*
 * Task context switch — Linux-style switch_to on struct cpu_context.
 */

#include <asm/assembler.h>

.section .text
.global switch_to
.global task_trampoline
.global finish_eret

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
    mov     x21, x0

    ldp     x1, x2, [x21, #248]
    msr     elr_el1, x1
    msr     spsr_el1, x2

    add     sp, x21, #272
    prepare_kstack_el0

    ldr     x30, [x21, #240]
    ldp     x28, x29, [x21, #224]
    ldp     x26, x27, [x21, #208]
    ldp     x24, x25, [x21, #192]
    ldp     x22, x23, [x21, #176]
    ldr     x20, [x21, #160]
    ldp     x18, x19, [x21, #144]
    ldp     x16, x17, [x21, #128]
    ldp     x14, x15, [x21, #112]
    ldp     x12, x13, [x21, #96]
    ldp     x10, x11, [x21, #80]
    ldp     x8, x9, [x21, #64]
    ldp     x6, x7, [x21, #48]
    ldp     x4, x5, [x21, #32]
    ldp     x2, x3, [x21, #16]
    ldp     x0, x1, [x21, #0]
    ldr     x21, [x21, #168]
    eret
