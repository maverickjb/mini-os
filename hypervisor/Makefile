# Standalone EL2 stub — does not build or modify mini-OS.
#
# Stage 1: prove EL2 boot + eret into EL1 on QEMU virt.

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -ffreestanding -nostdlib -nostartfiles -fno-builtin \
           -Wall -Wextra -O0 -g -fno-pie -fno-PIE
LDFLAGS := -T linker.ld -nostdlib -static -no-pie -Wl,--build-id=none

SRCS    := el2_entry.S
OBJS    := $(SRCS:.S=.o)

.PHONY: all clean run

all: hypervisor.elf hypervisor.bin

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

hypervisor.elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

hypervisor.bin: hypervisor.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) hypervisor.elf hypervisor.bin

QEMU    ?= qemu-system-aarch64
# Single CPU for stage 1. virt starts the -kernel image in EL2 on common setups.
QFLAGS  ?= -machine virt,gic-version=3,virtualization=on \
           -cpu cortex-a72 -smp 1 -nographic -m 128M

run: hypervisor.elf
	$(QEMU) $(QFLAGS) -kernel hypervisor.elf
