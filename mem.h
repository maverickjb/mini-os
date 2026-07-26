#ifndef MEM_H
#define MEM_H

/*
 * Kernel runs at a fixed high virtual address; RAM is at 0x40000000 phys
 * on QEMU virt (128 MiB).
 */
#define KERNEL_VIRT_BASE    0xffff800080000000UL
#define KERNEL_PHYS_BASE    0x40000000UL
#define PHYS_VIRT_OFFSET    (KERNEL_VIRT_BASE - KERNEL_PHYS_BASE)

#define MMIO_VIRT_BASE      0xffff800040000000UL
#define PHYS_MEM_BASE       KERNEL_PHYS_BASE
#define PHYS_MEM_SIZE       (128UL * 1024 * 1024)
#define PHYS_MEM_END        (PHYS_MEM_BASE + PHYS_MEM_SIZE)
#define VIRT_MEM_END        (KERNEL_VIRT_BASE + PHYS_MEM_SIZE)

#define __phys_to_virt(pa)  ((void *)((unsigned long)(pa) + PHYS_VIRT_OFFSET))
#define __virt_to_phys(va)  ((unsigned long)(va) - PHYS_VIRT_OFFSET)

#endif
