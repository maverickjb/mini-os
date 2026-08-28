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

SRCS    := kernel/head.S kernel/entry.S init/main.c kernel/smp.c \
           kernel/sched/core.c kernel/sched/idle.c \
           kernel/fork.c kernel/exit.c kernel/sys.c kernel/signal.c \
           kernel/pid.c kernel/irq.c kernel/time/tick.c kernel/time/timer.c \
           mm/page_alloc.c \
           fs/ramfs.c fs/initramfs.c fs/initramfs_blob.S fs/exec.c \
           fs/binfmt.c fs/open.c fs/stat.c fs/readdir.c fs/pipe.c fs/namei.c \
           fs/dcache.c fs/procfs.c fs/devnull.c fs/devtty.c \
           mm/mmap.c mm/uaccess.c lib/strnlen_user.c lib/memset.c lib/string.c \
           fs/read_write.c drivers/tty/serial.c drivers/tty/tty.c
OBJS    := $(SRCS:.c=.o)
OBJS    := $(OBJS:.S=.o)

.PHONY: all clean run initramfs

INITRAMFS_CPIO := initramfs/initramfs.cpio

all: initramfs mini-os.elf mini-os.bin

initramfs: $(INITRAMFS_CPIO)

USER_CC      ?= aarch64-unknown-linux-musl-gcc
USER_CFLAGS  := -Wall -Wextra -O0 -g -static -fno-pie -no-pie
USER_PATH    := $(HOME)/toolchains/aarch64-unknown-linux-musl/bin:$(PATH)

INIT_BIN     := initramfs/root/init
HELLO_BIN    := initramfs/root/bin/hello
ECHO_BIN     := initramfs/root/bin/echo
BUSYBOX_SRC  ?= initramfs/busybox
BUSYBOX_BIN  := initramfs/root/bin/busybox

$(INITRAMFS_CPIO): $(INIT_BIN) $(HELLO_BIN) $(ECHO_BIN) $(BUSYBOX_BIN) initramfs/etc/profile
	mkdir -p initramfs/root/tmp initramfs/root/etc
	cp -f initramfs/etc/profile initramfs/root/etc/profile
	test -f initramfs/root/etc/passwd || printf 'root:x:0:0:root:/:/bin/sh\n' > initramfs/root/etc/passwd
	cd initramfs/root && find . -print | cpio -o -H newc --quiet > ../initramfs.cpio

$(BUSYBOX_BIN): $(BUSYBOX_SRC)
	mkdir -p initramfs/root/bin
	cp -f $< $@

$(INIT_BIN): initramfs/src/init.c
	mkdir -p initramfs/root
	PATH="$(USER_PATH)" $(USER_CC) $(USER_CFLAGS) -o $@ $<

$(HELLO_BIN): initramfs/src/hello.c
	mkdir -p initramfs/root/bin initramfs/root/etc
	PATH="$(USER_PATH)" $(USER_CC) $(USER_CFLAGS) -o $@ $<
	printf 'hello from open\n' > initramfs/root/msg.txt
	printf 'root:x:0:0:root:/:/bin/sh\n' > initramfs/root/etc/passwd

$(ECHO_BIN): initramfs/src/echo.c
	mkdir -p initramfs/root/bin
	PATH="$(USER_PATH)" $(USER_CC) $(USER_CFLAGS) -o $@ $<

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