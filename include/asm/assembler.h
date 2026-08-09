#ifndef __ASM_ASSEMBLER_H
#define __ASM_ASSEMBLER_H

#include <asm/asm-offsets.h>

/*
 * Set SP_EL1 to current task kernel stack top when returning to EL0.
 * Clobbers x9–x11. Call only before those user regs are restored.
 */
.macro prepare_kstack_el0
	mrs	x11, spsr_el1
	and	x11, x11, #0xf
	cbnz	x11, 999f
	adr	x10, cpu_current_export
	ldr	x10, [x10]
	cbz	x10, 999f
	ldr	x9, [x10, #TASK_stack]
	cbz	x9, 999f
	add	x9, x9, #INIT_STACK_SIZE
	mov	sp, x9
999:
.endm

#endif /* __ASM_ASSEMBLER_H */
