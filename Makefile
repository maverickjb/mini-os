# miniSMP — AArch64 bare-metal SMP playground for QEMU virt

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
AS      := $(CROSS)as
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -ffreestanding -nostdlib -nostartfiles -fno-builtin \
           -Wall -Wextra -O2 -g
ASFLAGS :=
LDFLAGS := -T linker.ld -nostdlib

SRCS    := boot.S kernel.c uart.c
OBJS    := $(SRCS:.c=.o)
OBJS    := $(OBJS:.S=.o)

.PHONY: all clean

all: mini-os.elf mini-os.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

mini-os.elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

mini-os.bin: mini-os.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) mini-os.elf mini-os.bin
