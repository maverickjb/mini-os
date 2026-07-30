/*
 * EL1 exception vectors — IRQ dispatches to handle_arch_irq().
 */

#include "mem.h"

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
 * struct pt_regs is saved on the EL1 stack; x0-x30, elr_el1, spsr_el1.
 */
sync_el0_entry:
    ldr     x25, =_stack_top
    mov     sp, x25
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
    bl      syscall_handler

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
    eret

irq_entry:
    sub     sp, sp, #256
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

    bl      handle_arch_irq

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
    add     sp, sp, #256
    eret

.global vectors_init
vectors_init:
    ldr     x0, =vectors
    msr     VBAR_EL1, x0
    isb
    ret
