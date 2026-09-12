/*
 * EL2 ↔ EL1 via HVC (no mini-OS yet).
 *
 *   EL2 boot → eret → EL1 → hvc #0 → EL2 → print → eret → EL1 continues
 *
 * Note: on QEMU virt, ELR_EL2 for HVC already points at the following
 * instruction, so the handler must NOT add 4 (that would skip the next insn).
 */

#define UART_DR		0x09000000
#define UART_FR		0x09000018

#define SPSR_EL1h	0x3c5

#define ESR_EC_HVC64	0x16
#define ESR_EC_SHIFT	26

.section .text.boot, "ax"
.global el2_start
el2_start:
	mrs	x0, mpidr_el1
	and	x0, x0, #0xff
	cbnz	x0, park

	mrs	x0, CurrentEL
	lsr	x0, x0, #2
	and	x0, x0, #3
	cmp	x0, #2
	b.eq	at_el2
	b	park

at_el2:
	ldr	x0, =el2_stack_top
	mov	sp, x0

	adr	x0, msg_el2
	bl	uart_puts

	adr	x0, el2_vectors
	msr	vbar_el2, x0
	isb

	mrs	x0, cnthctl_el2
	orr	x0, x0, #3
	msr	cnthctl_el2, x0
	msr	cntvoff_el2, xzr

	mov	x0, #(1 << 31)			/* HCR_EL2.RW */
	msr	hcr_el2, x0
	msr	sctlr_el1, xzr

	ldr	x0, =el1_stack_top
	msr	sp_el1, x0

	ldr	x0, =el1_entry
	msr	elr_el2, x0
	mov	x0, #SPSR_EL1h
	msr	spsr_el2, x0
	isb
	eret

.global el1_entry
el1_entry:
	ldr	x0, =el1_stack_top
	mov	sp, x0

	adr	x0, msg_el1
	bl	uart_puts

	mov	x0, #0
	hvc	#0

	adr	x0, msg_el1_after
	bl	uart_puts
	b	park

park:
	wfi
	b	park

.global uart_puts
uart_puts:
	ldr	x1, =UART_DR
1:
	ldrb	w2, [x0], #1
	cbz	w2, 3f
2:
	ldr	w3, [x1, #(UART_FR - UART_DR)]
	tbnz	w3, #5, 2b
	str	w2, [x1]
	b	1b
3:
	ret

	.align 11
.global el2_vectors
el2_vectors:
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_sync_el1
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid
	.align 7
	b	el2_invalid

el2_invalid:
	adr	x0, msg_el2_bad
	bl	uart_puts
	b	park

el2_sync_el1:
	sub	sp, sp, #64
	stp	x0, x1, [sp, #0]
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x29, x30, [sp, #48]

	mrs	x0, esr_el2
	lsr	x1, x0, #ESR_EC_SHIFT
	and	x1, x1, #0x3f
	cmp	x1, #ESR_EC_HVC64
	b.ne	1f

	adr	x0, msg_hvc
	bl	uart_puts

	/* ELR already points past HVC on QEMU — do not add 4. */

	ldp	x0, x1, [sp, #0]
	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x29, x30, [sp, #48]
	add	sp, sp, #64
	eret

1:
	adr	x0, msg_el2_sync
	bl	uart_puts
	b	park

.section .rodata
msg_el2:
	.asciz	"[hyp] running at EL2\r\n"
msg_el1:
	.asciz	"[hyp] now at EL1, issuing HVC\r\n"
msg_hvc:
	.asciz	"[hyp] HVC received\r\n"
msg_el1_after:
	.asciz	"[hyp] back at EL1 after HVC\r\n"
msg_el2_bad:
	.asciz	"[hyp] unexpected EL2 exception\r\n"
msg_el2_sync:
	.asciz	"[hyp] EL1 sync but not HVC\r\n"
