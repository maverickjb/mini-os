# mini-os — AArch64 bare-metal SMP playground for QEMU virt

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
AS      := $(CROSS)as
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -ffreestanding -nostdlib -nostartfiles -fno-builtin \
           -Wall -Wextra -O0 -g -fno-pie -fno-PIE -I. -I fs
ASFLAGS :=
LDFLAGS := -T linker.ld -nostdlib -static -no-pie -Wl,--build-id=none \
           -Wl,--entry=0x40000000

SRCS    := boot.S vectors.S context.S mmu_enable.S kernel.c smp.c sched.c task.c irq.c time.c uart.c page_alloc.c mmu.c fs/ramfs.c
OBJS    := $(SRCS:.c=.o)
OBJS    := $(OBJS:.S=.o)

.PHONY: all clean run

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

QEMU    ?= qemu-system-aarch64
QFLAGS  ?= -machine virt,gic-version=3 -cpu cortex-a72 -smp 4 -nographic

run: mini-os.elf
	$(QEMU) $(QFLAGS) -kernel mini-os.elf
