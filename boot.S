/*
 * miniSMP — AArch64 bare-metal entry for QEMU virt
 *
 * CPU0 sets up the stack and enters C. Secondaries park until cpu_release[]
 * is written, then jump to the release address (see smp.c / cpu_up).
 */

.section .text.boot, "ax"
.global _start

_start:
    /* MPIDR_EL1[7:0] = Aff0 = CPU id within cluster */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff
    cbnz    x0, secondary_park

    /* CPU0: set stack (grows down from _stack_top) */
    ldr     x1, =_stack_top
    mov     sp, x1

    /* Clear .bss */
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
1:
    cmp     x1, x2
    b.ge    2f
    str     xzr, [x1], #8
    b       1b
2:
    bl      kernel_main

    /* Should not return; park if it does */
hang:
    wfe
    b       hang

secondary_park:
    adr     x1, cpu_release
    ldr     x2, [x1, x0, lsl #3]
    cbz     x2, 1f
    br      x2
1:
    wfe
    b       secondary_park

.global secondary_startup
secondary_startup:
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff

    ldr     x1, =__per_cpu_stacks
    ldr     x2, =__per_cpu_stack_size
    mul     x3, x0, x2
    add     x1, x1, x3
    add     sp, x1, x2

    bl      secondary_main

secondary_hang:
    wfe
    b       secondary_hang
