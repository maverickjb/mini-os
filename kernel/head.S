/*
 * miniSMP — AArch64 bare-metal entry for QEMU virt
 */

#include <asm/memory.h>

#define PTE_TABLE       (3 << 0)
#define PTE_BLOCK       (1 << 0)
#define PTE_AF          (1 << 10)
#define PTE_ATTR_DEV    (1 << 2)
#define PTE_NX          (3 << 53)

.macro call_phys fn
    ldr     x10, =\fn
    ldr     x11, =PHYS_VIRT_OFFSET
    sub     x10, x10, x11
    blr     x10
.endm

.macro block_pte pa, dev
    mov     x8, #PTE_BLOCK
    orr     x8, x8, #PTE_AF
    .if \dev == 1
    orr     x8, x8, #PTE_ATTR_DEV
    orr     x8, x8, #PTE_NX
    .endif
    ldr     x7, =\pa
    bic     x7, x7, #0x1fffff
    orr     x8, x8, x7
.endm

.macro map_l2_blocks l2_label, pa_base, size, dev
    mov     x4, #0
1:
    block_pte 0, \dev
    add     x8, x8, x4
    .if \pa_base != 0
    ldr     x7, =\pa_base
    add     x8, x8, x7
    .endif
    adr     x9, \l2_label
    lsr     x6, x4, #21
    str     x8, [x9, x6, lsl #3]
    add     x4, x4, #(2 * 1024 * 1024)
    ldr     x10, =\size
    cmp     x4, x10
    b.lt    1b
.endm

.macro pte_store table, index, target
    adr     x0, \table
    adr     x1, \target
    orr     x1, x1, #PTE_TABLE
    mov     x2, #\index
    str     x1, [x0, x2, lsl #3]
.endm

.macro mmu_build_tables
    pte_store boot_pgd, 0x100, boot_l1
    pte_store boot_l1, 1, boot_l2_mmio
    pte_store boot_l1, 2, boot_l2_kern

    map_l2_blocks boot_l2_mmio, 0, 0x40000000, 1
    map_l2_blocks boot_l2_kern, PHYS_MEM_BASE, PHYS_MEM_SIZE, 0

    /* TTBR0 (T0SZ=25) starts at level 1 — no PGD. */
    pte_store boot_l1_id, 0, boot_l2_mmio
    pte_store boot_l1_id, 1, boot_l2_kern
.endm

.macro mmu_turn_on cont
    /* T0SZ=25, T1SZ=16, IPS=2, IRGN/ORGN WB RA */
    ldr     x4, =((2 << 32) | (16 << 16) | 25 | (3 << 10) | (3 << 8))
    msr     tcr_el1, x4

    mov     x3, #0xee
    msr     mair_el1, x3
    isb

    adr     x0, boot_l1_id
    msr     ttbr0_el1, x0
    adr     x0, boot_pgd
    msr     ttbr1_el1, x0

    tlbi    vmalle1
    dsb     sy
    isb

    adr     x6, \cont
    ldr     x7, =PHYS_VIRT_OFFSET
    add     x6, x6, x7

    mrs     x5, sctlr_el1
    ldr     x8, =0x100d
    bic     x5, x5, #(1 << 1)
    bic     x5, x5, #(1 << 19)
    orr     x5, x5, x8
    dsb     sy
    msr     sctlr_el1, x5
    isb
    br      x6
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

    /* Enable FP/ASIMD at EL0/EL1 (printf/va_list may spill q0–q7). */
    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
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

    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
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

.global vectors_init
vectors_init:
    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb
    ret

.global mmu_enable
.global mmu_enable_secondary

mmu_enable:
    mmu_build_tables
    dsb     ishst
    isb
    mmu_turn_on boot_cont

mmu_enable_secondary:
    mmu_turn_on secondary_cont

.section .data.boot, "aw"
.align 12
boot_pgd:
    .skip 4096
.align 12
boot_l1:
    .skip 4096
.align 12
boot_l2_mmio:
    .skip 4096
.align 12
boot_l2_kern:
    .skip 4096
.align 12
boot_l1_id:
    .skip 4096
