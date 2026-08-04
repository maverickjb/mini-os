# mini-os — AArch64 bare-metal SMP playground for QEMU virt

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
AS      := $(CROSS)as
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -ffreestanding -nostdlib -nostartfiles -fno-builtin \
           -Wall -Wextra -O0 -g -fno-pie -fno-PIE \
           -I. -I include -I fs -I mm -I kernel
ASFLAGS :=
LDFLAGS := -T linker.ld -nostdlib -static -no-pie -Wl,--build-id=none \
           -Wl,--entry=0x40000000

SRCS    := boot.S vectors.S context.S mmu_enable.S init/main.c kernel/smp.c \
           kernel/sched/core.c kernel/sched/idle.c \
           kernel/fork.c kernel/exit.c kernel/sys.c kernel/irq.c kernel/time/tick.c \
           mm/page_alloc.c \
           fs/ramfs.c fs/initramfs.c fs/initramfs_blob.S fs/exec.c \
           fs/binfmt.c mm/mmap.c mm/uaccess.c fs/read_write.c drivers/tty/serial.c
OBJS    := $(SRCS:.c=.o)
OBJS    := $(OBJS:.S=.o)

.PHONY: all clean run initramfs

INITRAMFS_CPIO := initramfs/initramfs.cpio

all: initramfs mini-os.elf mini-os.bin

initramfs: $(INITRAMFS_CPIO)

INIT_BIN  := initramfs/root/init
HELLO_BIN := initramfs/root/bin/hello

$(INITRAMFS_CPIO): $(INIT_BIN) $(HELLO_BIN)
	cd initramfs/root && find . -print | cpio -o -H newc --quiet > ../initramfs.cpio

INIT_CFLAGS := -ffreestanding -nostdlib -nostartfiles -fno-builtin \
               -static -Wl,-e,_start

$(INIT_BIN): initramfs/src/init.c
	mkdir -p initramfs/root
	$(CC) $(INIT_CFLAGS) -o $@ $<

$(HELLO_BIN): initramfs/src/hello.c
	mkdir -p initramfs/root/bin
	$(CC) $(INIT_CFLAGS) -o $@ $<

fs/initramfs_blob.o: $(INITRAMFS_CPIO)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

mini-os.elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

mini-os.bin: mini-os.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) mini-os.elf mini-os.bin initramfs/initramfs.cpio
	rm -rf initramfs/root

QEMU    ?= qemu-system-aarch64
QFLAGS  ?= -machine virt,gic-version=3 -cpu cortex-a72 -smp 4 -nographic

run: mini-os.elf
	$(QEMU) $(QFLAGS) -kernel mini-os.elf
