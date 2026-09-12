# Standalone EL2 stub — boots mini-OS as EL1 guest.
#
# Layout: guest (mini-os.bin) @ 0x40000000, hypervisor @ 0x44000000

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc

CFLAGS  := -ffreestanding -nostdlib -nostartfiles -fno-builtin \
           -Wall -Wextra -O0 -g -fno-pie -fno-PIE
LDFLAGS := -T linker.ld -nostdlib -static -no-pie -Wl,--build-id=none

GUEST_BIN := ../mini-os.bin
SRCS      := el2_entry.S guest.S
OBJS      := $(SRCS:.S=.o)

.PHONY: all clean run guest

all: hypervisor.elf

guest: $(GUEST_BIN)

$(GUEST_BIN):
	$(MAKE) -C .. mini-os.bin

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# Rebuild guest.o whenever mini-os.bin changes.
guest.o: guest.S $(GUEST_BIN)

hypervisor.elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f $(OBJS) hypervisor.elf
	rm -f guest_placeholder.o

QEMU    ?= qemu-system-aarch64
QFLAGS  ?= -machine virt,gic-version=3,virtualization=on \
           -cpu cortex-a72 -smp 1 -nographic -m 128M

run: hypervisor.elf
	$(QEMU) $(QFLAGS) -kernel hypervisor.elf
