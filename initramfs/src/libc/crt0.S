/*
 * CRT0 — process entry for mini-os userspace.
 */

.section .text
.global _start

_start:
    mov     x0, xzr
    mov     x1, xzr
    bl      main
    bl      _exit
