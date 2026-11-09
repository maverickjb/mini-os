/*
 * EL2 boots hypervisor @ 0x44000000, then eret into mini-OS @ 0x40000000.
 *
 * Guest PSCI is HVC. CPU_ON writes a per-CPU entry and SEV; parked
 * secondaries eret to that address (mini-os secondary_startup, MMU off).
 *
 * Note: on QEMU virt, ELR_EL2 for HVC already points past the HVC insn.
 */

#define UART_DR		0x09000000
#define UART_FR		0x09000018

#define SPSR_EL1h	0x3c5

#define ESR_EC_HVC64	0x16
#define ESR_EC_SHIFT	26

#define NR_CPUS			4
#define PSCI_CPU_ON64		0xc4000003
#define PSCI_SUCCESS		0
#define PSCI_INVALID_PARAMETERS	-2
#define PSCI_ALREADY_ON		-4
#define PSCI_INVALID_ADDRESS	-9

#define GUEST_BASE		0x40000000
#define HYP_BASE		0x44000000

.macro el2_setup_el1
	adr	x10, el2_vectors
	msr	vbar_el2, x10
	isb
	mrs	x10, cnthctl_el2
	orr	x10, x10, #3
	msr	cnthctl_el2, x10
	msr	cntvoff_el2, xzr
	mov	x10, #(1 << 31)			/* HCR_EL2.RW */
	msr	hcr_el2, x10
	msr	sctlr_el1, xzr
.endm

.section .text.boot, "ax"
.global el2_start
el2_start:
	mrs	x0, mpidr_el1
	and	x0, x0, #0xff
	cbnz	x0, secondary_wait

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

	/* guest @ 0x40000000, hyp @ 0x44000000, guest must not overlap hyp */
	ldr	x0, =guest_start
	ldr	x1, =GUEST_BASE
	cmp	x0, x1
	b.ne	layout_bad
	ldr	x0, =el2_start
	ldr	x1, =HYP_BASE
	cmp	x0, x1
	b.ne	layout_bad
	ldr	x0, =guest_end
	ldr	x1, =HYP_BASE
	cmp	x0, x1
	b.hi	layout_bad
	adr	x0, msg_layout_ok
	bl	uart_puts
	b	layout_done
layout_bad:
	adr	x0, msg_layout_bad
	bl	uart_puts
	b	park
layout_done:

	el2_setup_el1

	/*
	 * Guest sets SP itself in _start; seed SP_EL1 anyway.
	 * ELR = physical entry of mini-OS (same as former QEMU -kernel PC).
	 */
	ldr	x0, =el1_stack_top
	msr	sp_el1, x0

	adr	x0, msg_enter_guest
	bl	uart_puts

	ldr	x0, =guest_start		/* 0x40000000 */
	msr	elr_el2, x0
	mov	x0, #SPSR_EL1h
	msr	spsr_el2, x0
	isb
	eret

/*
 * Secondaries start at EL2 with QEMU -smp N. Wait for CPU0's CPU_ON,
 * then enter the guest at the PSCI entry (MMU off).
 */
secondary_wait:
	mrs	x0, mpidr_el1
	and	x0, x0, #0xff
	cmp	x0, #NR_CPUS
	b.hs	park

	ldr	x1, =cpu_on_entry
1:
	ldr	x2, [x1, x0, lsl #3]
	cbnz	x2, 2f
	wfe
	b	1b
2:
	ldr	x3, =cpu_on_ctx
	ldr	x3, [x3, x0, lsl #3]

	el2_setup_el1
	ldr	x4, =el1_stack_top
	msr	sp_el1, x4
	msr	elr_el2, x2
	mov	x4, #SPSR_EL1h
	msr	spsr_el2, x4
	mov	x0, x3				/* PSCI context_id */
	isb
	eret

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
	b.ne	el2_sync_not_hvc

	ldp	x0, x1, [sp, #0]
	ldr	x2, =PSCI_CPU_ON64
	cmp	x0, x2
	b.eq	psci_cpu_on

	adr	x0, msg_hvc
	bl	uart_puts

	ldp	x0, x1, [sp, #0]
	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x29, x30, [sp, #48]
	add	sp, sp, #64
	eret

/*
 * x0 = CPU_ON, x1 = target MPIDR (affinity0 = cpu id), x2 = entry, x3 = ctx
 */
psci_cpu_on:
	ldp	x0, x1, [sp, #0]
	ldp	x2, x3, [sp, #16]
	and	x1, x1, #0xff
	cmp	x1, #0
	b.eq	psci_bad_param
	cmp	x1, #NR_CPUS
	b.hs	psci_bad_param

	ldr	x4, =GUEST_BASE
	cmp	x2, x4
	b.lo	psci_bad_addr
	ldr	x4, =HYP_BASE
	cmp	x2, x4
	b.hs	psci_bad_addr

	ldr	x4, =cpu_on_entry
	ldr	x5, [x4, x1, lsl #3]
	cbnz	x5, psci_already_on
	str	x2, [x4, x1, lsl #3]
	ldr	x4, =cpu_on_ctx
	str	x3, [x4, x1, lsl #3]
	dsb	sy

	/*
	 * QEMU virt + virtualization: PSCI conduit is SMC, secondaries
	 * start powered off. Forward CPU_ON so TCG runs that vCPU at
	 * our EL2 wait stub; the stub eret's into the guest entry.
	 */
	mov	x5, x1				/* cpu */
	mov	x6, x3				/* context_id */
	ldr	x0, =PSCI_CPU_ON64
	mov	x1, x5
	ldr	x2, =secondary_wait
	mov	x3, x6
	smc	#0
	cbz	x0, 1f
	ldr	x4, =cpu_on_entry
	str	xzr, [x4, x5, lsl #3]
	b	psci_return
1:
	adr	x0, msg_cpu_on
	bl	uart_puts
	mov	x0, #PSCI_SUCCESS
	b	psci_return

psci_bad_param:
	mov	x0, #PSCI_INVALID_PARAMETERS
	b	psci_return
psci_bad_addr:
	mov	x0, #PSCI_INVALID_ADDRESS
	b	psci_return
psci_already_on:
	mov	x0, #PSCI_ALREADY_ON
	b	psci_return

psci_return:
	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x29, x30, [sp, #48]
	add	sp, sp, #64
	eret

el2_sync_not_hvc:
	adr	x0, msg_el2_sync
	bl	uart_puts
	b	park

.section .data
.align 3
cpu_on_entry:
	.quad	0, 0, 0, 0
cpu_on_ctx:
	.quad	0, 0, 0, 0

.section .rodata
msg_el2:
	.asciz	"[hyp] running at EL2\r\n"
msg_layout_ok:
	.asciz	"[hyp] layout: guest@0x40000000 hyp@0x44000000\r\n"
msg_layout_bad:
	.asciz	"[hyp] layout check FAILED\r\n"
msg_enter_guest:
	.asciz	"[hyp] eret -> mini-OS guest\r\n"
msg_cpu_on:
	.asciz	"[hyp] PSCI CPU_ON\r\n"
msg_hvc:
	.asciz	"[hyp] HVC received\r\n"
msg_el2_bad:
	.asciz	"[hyp] unexpected EL2 exception\r\n"
msg_el2_sync:
	.asciz	"[hyp] EL1 sync but not HVC\r\n"
