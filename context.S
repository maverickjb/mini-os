/*
 * Task context switch — save/restore callee-saved registers.
 */

.section .text
.global cpu_switch_to
.global task_trampoline

/*
 * void cpu_switch_to(struct task_struct *prev, struct task_struct *next)
 * saved_sp is the first field after pid/state in task_struct — offset 16.
 */
.cpu_switch_to_offset_saved_sp = 16

cpu_switch_to:
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x29, x30, [sp, #-16]!

    mov     x10, sp
    str     x10, [x0, #.cpu_switch_to_offset_saved_sp]

    ldr     x10, [x1, #.cpu_switch_to_offset_saved_sp]
    mov     sp, x10

    ldp     x29, x30, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ret

/*
 * New task entry: x19 = arg, x20 = fn (set up on the task's initial stack).
 */
task_trampoline:
    mov     x0, x19
    blr     x20
    b       schedule
