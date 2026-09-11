/*
 * Minimal EL2 “hypervisor” stub for QEMU virt (AArch64).
 *
 * Stage 1 goal only:
 *   boot in EL2 → print → eret → EL1 → print → park
 *
 * Does NOT load mini-OS yet. mini-OS stays untouched.
 */

OUTPUT_ARCH(aarch64)
ENTRY(el2_start)

/* Same load address QEMU -kernel uses; fine while guest OS is not linked in. */
HYP_PHYS_BASE = 0x40000000;

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = HYP_PHYS_BASE;

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    } :text

    .rodata : {
        *(.rodata .rodata.*)
    } :text

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    .stack (NOLOAD) : {
        . = ALIGN(16);
        . += 0x4000;
        el2_stack_top = .;
        . += 0x4000;
        el1_stack_top = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}
