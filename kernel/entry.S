/*
 * Low-level kernel entry — exceptions, switch_to, trampoline, return to EL0.
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

.section .text.boot, "ax"
.align 11
.global vectors
vectors:
    /* Current EL, SP_EL0 */
    .align 7
    b       exc_hang
    .align 7
    b       exc_hang
    .align 7
    b       exc_hang
    .align 7
    b       exc_hang

    /* Current EL, SP_ELx */
    .align 7
    b       exc_hang
    .align 7
    b       irq_entry
    .align 7
    b       exc_hang
    .align 7
    b       exc_hang

    /* Lower EL, AArch64 */
    .align 7
    b       sync_el0_entry
    .align 7
    b       irq_entry
    .align 7
    b       exc_hang
    .align 7
    b       exc_hang

    /* Lower EL, AArch32 */
    .align 7
    b       exc_hang
    .align 7
    b       exc_hang
    .align 7
    b       exc_hang
    .align 7
    b       exc_hang

exc_hang:
    wfe
    b       exc_hang

/*
 * EL0 synchronous exception (SVC #0 syscall).
 * SP_EL1 must already point at current task kernel stack top (set on last eret).
 */
sync_el0_entry:
    sub     sp, sp, #(PT_REGS_SIZE + SYNC_C_STACK)
    stp     x19, x20, [sp, #0]
    add     x19, sp, #SYNC_C_STACK

    stp     x0, x1, [x19, #0]
    stp     x2, x3, [x19, #16]
    stp     x4, x5, [x19, #32]
    stp     x6, x7, [x19, #48]
    stp     x8, x9, [x19, #64]
    stp     x10, x11, [x19, #80]
    stp     x12, x13, [x19, #96]
    stp     x14, x15, [x19, #112]
    stp     x16, x17, [x19, #128]
    /*
     * x19/x20 were stashed at [sp]; x19 now points at pt_regs.
     * Save x21 before reusing it as scratch — musl mallocng keeps
     * sizeclass in w21 across mmap().
     */
    stp     x22, x23, [x19, #176]
    str     x21, [x19, #168]
    ldp     x21, x22, [sp, #0]
    stp     x18, x21, [x19, #144]
    str     x22, [x19, #160]
    stp     x24, x25, [x19, #192]
    stp     x26, x27, [x19, #208]
    stp     x28, x29, [x19, #224]
    str     x30, [x19, #240]

    mrs     x21, elr_el1
    mrs     x22, spsr_el1
    stp     x21, x22, [x19, #248]

    mrs     x23, esr_el1
    ubfx    x23, x23, #26, #6
    cmp     x23, #0x15
    b.ne    sync_el0_fault

    mov     x0, x19
    bl      syscall_handler

    /*
     * Do not raise SP before finish_eret: with IRQs enabled that would let
     * an IRQ frame overlay the pt_regs at x19. finish_eret sets SP from x0.
     */
    mov     x0, x19
    bl      finish_eret

sync_el0_fault:
    add     x19, sp, #SYNC_C_STACK
    mov     x0, x19
    bl      do_data_abort
    /*
     * Fault resolved (demand page filled). Retry the faulting
     * instruction — ELR still points at it.
     */
    mov     x0, x19
    bl      finish_eret

/*
 * IRQ from EL0 or EL1 — pt_regs on current kernel stack (SP_EL1).
 */
irq_entry:
    sub     sp, sp, #272
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30, [sp, #240]

    mrs     x21, elr_el1
    mrs     x22, spsr_el1
    stp     x21, x22, [sp, #248]

    mov     x0, sp
    bl      handle_arch_irq

    /*
     * Stage ELR/SPSR and fix SP_EL1 for EL0 return before restoring user
     * regs. A bl here would clobber user x30 (LR) — that sent EL0 ret into
     * this eret and faulted with elr at the IRQ exit stub.
     */
    ldp     x21, x22, [sp, #248]
    msr     elr_el1, x21
    msr     spsr_el1, x22

    mov     x21, sp
    add     sp, sp, #272
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
