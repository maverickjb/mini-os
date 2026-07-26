/*
 * Linker script for miniSMP on QEMU virt (AArch64)
 *
 * Linked at KERNEL_VIRT_BASE, loaded at KERNEL_PHYS_BASE by QEMU -kernel.
 */

OUTPUT_ARCH(aarch64)
ENTRY(_start)

KERNEL_VIRT_BASE = 0xffff800080000000;
KERNEL_PHYS_BASE = 0x40000000;

SECTIONS
{
    . = KERNEL_VIRT_BASE;

    .text : AT(KERNEL_PHYS_BASE) {
        _text = .;
        KEEP(*(.text.boot))
        *(.text .text.*)
        _etext = .;
    }

    .databoot : AT(KERNEL_PHYS_BASE + (ADDR(.databoot) - KERNEL_VIRT_BASE)) {
        *(.data.boot)
    }

    .rodata : AT(KERNEL_PHYS_BASE + (ADDR(.rodata) - KERNEL_VIRT_BASE)) {
        *(.rodata .rodata.*)
    }

    .data : AT(KERNEL_PHYS_BASE + (ADDR(.data) - KERNEL_VIRT_BASE)) {
        *(.data .data.*)
    }

    .bss : AT(KERNEL_PHYS_BASE + (ADDR(.bss) - KERNEL_VIRT_BASE)) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    . += 0x10000;
    _stack_top = .;

    . = ALIGN(16);
    __per_cpu_stack_size = 0x4000;
    __per_cpu_stacks = .;
    . += __per_cpu_stack_size * 4;

    . = ALIGN(4096);
    __alloc_start = .;

    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame*)
        *(.gnu.hash)
        *(.gnu.build-id)
        *(.rela.dyn)
        *(.dynamic)
        *(.interp)
    }
}
