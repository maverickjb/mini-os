/*
 * Enable EL1 MMU — PGD -> L1 -> L2 -> 2 MiB blocks.
 *
 * TTBR0: temporary identity map for the MMU-on transition.
 * TTBR1: kernel at KERNEL_VIRT_BASE, MMIO at MMIO_VIRT_BASE.
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

.section .text.boot, "ax"
.global mmu_enable
.global mmu_enable_secondary

mmu_enable:
    mmu_build_tables
    dsb     ishst
    isb
    mmu_turn_on boot_cont

mmu_enable_secondary:
    mmu_turn_on secondary_cont
