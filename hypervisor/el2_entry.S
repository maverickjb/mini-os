/*
 * Stage 1: EL2 boot stub → eret into EL1.
 *
 * Linked at PA 0x40000000 (QEMU virt -kernel). MMU off; UART at 0x09000000.
 */

#define UART_DR     0x09000000
#define UART_FR     0x09000018
#define UART_FR_TXFF (1 << 5)

/* SPSR: EL1h, IRQ/FIQ/SError/Debug masked */
#define SPSR_EL1h   0x3c5

.section .text.boot, "ax"
.global el2_start
el2_start:
    /* Only CPU0 continues; others park. */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff
    cbnz    x0, park

    /* Which exception level did firmware drop us in? */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    and     x0, x0, #3

    cmp     x0, #2
    b.eq    at_el2
    cmp     x0, #1
    b.eq    already_el1

    /* EL3 or unexpected: park (QEMU virt normally gives EL2 or EL1). */
    b       park

already_el1:
    /*
     * Image started in EL1 — still useful: prove UART + EL1 path.
     * (No EL2 work possible without resetting / different machine config.)
     */
    ldr     x0, =el1_stack_top
    mov     sp, x0
    adr     x0, msg_el1_direct
    bl      uart_puts
    b       park

at_el2:
    ldr     x0, =el2_stack_top
    mov     sp, x0

    adr     x0, msg_el2
    bl      uart_puts

    /* Allow EL1/EL0 to use the generic timer (harmless for this stub). */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #3
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr

    /* EL1 is AArch64; no Stage-2 VM yet (HCR.VM = 0). */
    mov     x0, #(1 << 31)          /* HCR_EL2.RW */
    msr     hcr_el2, x0

    /* Minimal EL1 system registers (MMU off). */
    msr     sctlr_el1, xzr
    msr     spsr_el2, xzr

    ldr     x0, =el1_stack_top
    msr     sp_el1, x0

    adr     x0, el1_entry
    msr     elr_el2, x0

    mov     x0, #SPSR_EL1h
    msr     spsr_el2, x0

    isb
    eret
    /* not reached */

.global el1_entry
el1_entry:
    /* SP_EL1 already set via msr sp_el1 before eret. */
    adr     x0, msg_el1
    bl      uart_puts
    b       park

park:
    wfi
    b       park

/*
 * Early UART (PL011), MMU off, physical addresses.
 * x0 = NUL-terminated string. Clobbers x1–x3.
 */
uart_puts:
    ldr     x1, =UART_DR
1:
    ldrb    w2, [x0], #1
    cbz     w2, 3f
2:
    ldr     w3, [x1, #(UART_FR - UART_DR)]
    tbnz    w3, #5, 2b              /* TXFF */
    str     w2, [x1]
    b       1b
3:
    ret

.section .rodata
msg_el2:
    .asciz  "[hyp] running at EL2\r\n"
msg_el1:
    .asciz  "[hyp] eret -> EL1 OK\r\n"
msg_el1_direct:
    .asciz  "[hyp] started at EL1 (no EL2 drop)\r\n"
