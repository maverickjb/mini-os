/*
 * Embedded initramfs cpio image (see initramfs/ and Makefile).
 */

.section .init.ramfs, "a"
.global __initramfs_start
.global __initramfs_end
.align 4
__initramfs_start:
.incbin "initramfs/initramfs.cpio"
.align 4
__initramfs_end:
