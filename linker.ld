/*
 * Linker script for miniSMP on QEMU virt (AArch64)
 *
 * RAM starts at 0x40000000 on the virt machine.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* 64 KiB stack for CPU0, 16-byte aligned */
    . = ALIGN(16);
    . += 0x10000;
    _stack_top = .;

    /* Per-CPU stacks for secondaries (indexed by Aff0) */
    . = ALIGN(16);
    __per_cpu_stack_size = 0x4000;
    __per_cpu_stacks = .;
    . += __per_cpu_stack_size * 4;
}
