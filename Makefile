# mini-os — AArch64 bare-metal SMP playground for QEMU virt

CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
AS      := $(CROSS)as
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -ffreestanding -nostdlib -nostartfiles -fno-builtin \
           -Wall -Wextra -O0 -g -fno-pie -fno-PIE \
           -I. -I include -I fs -I mm -I kernel -I tests/kernel
ASFLAGS :=
LDFLAGS := -T linker.ld -nostdlib -static -no-pie -Wl,--build-id=none \
           -Wl,--entry=0x40000000

SRCS    := kernel/head.S kernel/entry.S init/main.c kernel/smp.c \
           kernel/sched/core.c kernel/sched/idle.c kernel/sched/wait.c \
           kernel/fork.c kernel/exit.c kernel/sys.c kernel/reboot.c kernel/psci.c \
           kernel/signal.c \
           kernel/pid.c kernel/irq.c kernel/time/tick.c kernel/time/timer.c \
           kernel/printk.c \
           mm/page_alloc.c mm/slub.c \
           fs/ramfs.c fs/initramfs.c fs/initramfs_blob.S fs/exec.c \
           fs/binfmt.c fs/open.c fs/stat.c fs/readdir.c fs/pipe.c fs/namei.c \
           fs/dcache.c fs/procfs.c fs/dev.c fs/devnull.c fs/devtty.c \
           fs/devconsole.c \
           mm/mmap.c mm/fault.c mm/uaccess.c lib/strnlen_user.c lib/memset.c lib/string.c \
           lib/vsnprintf.c lib/rbtree.c \
           tests/kernel/test_main.c tests/kernel/list_test.c \
           tests/kernel/rbtree_test.c tests/kernel/spinlock_test.c \
           tests/kernel/waitqueue_test.c tests/kernel/scheduler_test.c \
           tests/kernel/slub_test.c \
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

HELLO_BIN    := initramfs/root/bin/hello
BUSYBOX_SRC  ?= initramfs/busybox
BUSYBOX_BIN  := initramfs/root/bin/busybox
BUSYBOX_APPLETS := sh ash ls echo cat sleep ps uname true false pwd reboot poweroff halt

$(INITRAMFS_CPIO): $(HELLO_BIN) $(BUSYBOX_BIN) \
	initramfs/etc/profile initramfs/etc/inittab initramfs/etc/init.d/rcS
	mkdir -p initramfs/root/tmp initramfs/root/etc/init.d initramfs/root/sbin
	cp -f initramfs/etc/profile initramfs/root/etc/profile
	cp -f initramfs/etc/inittab initramfs/root/etc/inittab
	cp -f initramfs/etc/init.d/rcS initramfs/root/etc/init.d/rcS
	chmod +x initramfs/root/etc/init.d/rcS
	ln -sf bin/busybox initramfs/root/init
	ln -sf ../bin/busybox initramfs/root/sbin/init
	test -f initramfs/root/etc/passwd || printf 'root:x:0:0:root:/:/bin/sh\n' > initramfs/root/etc/passwd
	cd initramfs/root && find . -print | cpio -o -H newc --quiet > ../initramfs.cpio

$(BUSYBOX_BIN): $(BUSYBOX_SRC)
	mkdir -p initramfs/root/bin
	cp -f $< $@
	@for app in $(BUSYBOX_APPLETS); do \
		ln -sf busybox initramfs/root/bin/$$app; \
	done

$(HELLO_BIN): initramfs/src/hello.c
	mkdir -p initramfs/root/bin initramfs/root/etc
	PATH="$(USER_PATH)" $(USER_CC) $(USER_CFLAGS) -o $@ $<
	printf 'hello from open\n' > initramfs/root/msg.txt
	printf 'root:x:0:0:root:/:/bin/sh\n' > initramfs/root/etc/passwd

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