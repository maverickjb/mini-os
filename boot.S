/*
 * miniSMP — AArch64 bare-metal entry for QEMU virt
 *
 * All secondary CPUs park in WFI; CPU0 sets up the stack and enters C.
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
    wfe
    b       secondary_park
