/*
 * Guest payload at PA 0x40000000 — raw mini-os.bin (same image QEMU -kernel used).
 */
.section .guest, "ax"
.align 12
.global guest_start
guest_start:
	.incbin "../mini-os.bin"
.global guest_end
guest_end:
