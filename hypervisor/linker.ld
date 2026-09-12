/*
 * Hypervisor memory layout (QEMU virt):
 *
 *   0x40000000  .guest   — mini-os.bin
 *   0x44000000  .text…   — EL2 hypervisor (entry = el2_start)
 */

OUTPUT_ARCH(aarch64)
ENTRY(el2_start)

GUEST_PHYS_BASE = 0x40000000;
HYP_PHYS_BASE   = 0x44000000;

PHDRS
{
    guest PT_LOAD FLAGS(7); /* PF_R|PF_W|PF_X — guest runs in-place */
    text  PT_LOAD FLAGS(5); /* PF_R | PF_X */
}

SECTIONS
{
    .guest GUEST_PHYS_BASE : {
        guest_start = .;
        KEEP(*(.guest))
        . = ALIGN(4096);
        guest_end = .;
    } :guest

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
    } :text

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

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
