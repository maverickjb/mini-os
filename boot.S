/*
 * miniSMP — AArch64 bare-metal entry for QEMU virt
 */

#include "mem.h"

.macro call_phys fn
    ldr     x10, =\fn
    ldr     x11, =PHYS_VIRT_OFFSET
    sub     x10, x10, x11
    blr     x10
.endm

.section .text.boot, "ax"
.global _start

_start:
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff
    cbnz    x0, secondary_park

    ldr     x1, =_stack_top
    ldr     x2, =PHYS_VIRT_OFFSET
    sub     x1, x1, x2
    mov     sp, x1

    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
    ldr     x3, =PHYS_VIRT_OFFSET
    sub     x1, x1, x3
    sub     x2, x2, x3
1:
    cmp     x1, x2
    b.ge    2f
    str     xzr, [x1], #8
    b       1b
2:
    adr     x0, vectors
    msr     vbar_el1, x0
    isb

    call_phys mmu_enable

.global boot_cont
boot_cont:
    msr     ttbr0_el1, xzr
    tlbi    vmalle1
    dsb     sy
    isb

    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    ldr     x1, =_stack_top
    mov     sp, x1

    bl      start_kernel

hang:
    wfe
    b       hang

secondary_park:
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff

    ldr     x3, =PHYS_VIRT_OFFSET

    /* Per-CPU stack + VBAR before WFE (timer IRQ safe). */
    ldr     x1, =__per_cpu_stacks
    sub     x1, x1, x3
    ldr     x2, =__per_cpu_stack_size
    mul     x4, x0, x2
    add     x1, x1, x4
    add     sp, x1, x2

    ldr     x1, =vectors
    sub     x1, x1, x3
    msr     vbar_el1, x1
    isb

1:
    ldr     x1, =cpu_release
    sub     x1, x1, x3
    ldr     x2, [x1, x0, lsl #3]
    cbz     x2, 2f
    isb
    br      x2
2:
    wfe
    b       1b

.global secondary_startup
secondary_startup:
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff
    mov     x20, x0

    ldr     x1, =__per_cpu_stacks
    ldr     x2, =__per_cpu_stack_size
    ldr     x3, =PHYS_VIRT_OFFSET
    sub     x1, x1, x3
    mul     x3, x20, x2
    add     x1, x1, x3
    add     sp, x1, x2

    call_phys mmu_enable_secondary

.global secondary_cont
secondary_cont:
    msr     ttbr0_el1, xzr
    tlbi    vmalle1
    dsb     sy
    isb

    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    mov     x0, x20

    ldr     x1, =__per_cpu_stacks
    ldr     x2, =__per_cpu_stack_size
    mul     x3, x0, x2
    add     x1, x1, x3
    add     sp, x1, x2

    bl      secondary_main

secondary_hang:
    wfe
    b       secondary_hang
