# mini-os

A small AArch64 kernel for **learning how Linux-like kernels are put together**. It is not Linux, not POSIX-complete, and not meant for real hardware. It runs on QEMU `virt` and copies Linux *ideas* and *names* so you can follow the same mental model: boot, exceptions, scheduling, processes, virtual memory, a VFS, and system calls.

If you have read kernel source or a textbook chapter on “what a kernel does,” this tree is a walkable, runnable sketch of those pieces.

## What you can see here

| Linux idea | What mini-os does |
| --- | --- |
| Privilege levels | Kernel at EL1, user programs at EL0 |
| Exception vectors | SVC syscalls and IRQs in `kernel/entry.S` |
| Tasks / `task_struct` | Round-robin user and kernel threads |
| Fork / exec / exit / wait | Separate address spaces, ELF load, zombies |
| Process groups / sessions | `pgid` / `sid`, `setpgid`, `setsid`, TTY foreground pgrp |
| Signals | Pending bits, `sigaction`, mask, suspend, `sigreturn` |
| Page allocator + user maps | Buddy-style pages, user page tables, `mmap` / `brk` |
| VFS | Inodes, dentries, files, ramfs, pipes, symlinks |
| `/proc` | Minimal procfs for `ps` (`/proc/<pid>/stat`, `cmdline`) |
| Device nodes | Path hooks for `/dev/null`, `/dev/tty`, `/dev/console` |
| Initramfs + BusyBox | cpio rootfs; PID 1 is BusyBox `init` via `/init` → `busybox` |
| Kernel logging | `printk` / `pr_*` → UART; minimal `vsnprintf` |
| Synchronization | AArch64 spinlocks; wait queues for pipe/TTY sleep |

Many Linux syscall numbers exist in `include/linux/unistd.h`. Only the ones wired in `kernel/sys.c` actually work.

## Build and run

Needs two AArch64 compilers and QEMU:

```text
aarch64-linux-gnu-gcc                 # kernel (freestanding)
aarch64-unknown-linux-musl-gcc        # userspace test programs (static musl)
qemu-system-aarch64
```

The Makefile looks for the musl compiler on `PATH`, and also in `$HOME/toolchains/aarch64-unknown-linux-musl/bin`. Override with `USER_CC=...` if yours lives elsewhere.

Place a static AArch64 BusyBox binary at `initramfs/busybox` (musl, stripped is fine). The Makefile copies it into the cpio and creates applet symlinks.

```sh
make
make run
```

QEMU is started as:

```text
qemu-system-aarch64 -machine virt,gic-version=3 -cpu cortex-a72 -smp 4 -nographic -kernel mini-os.elf
```

Boot CPU0 brings up three secondary CPUs, unpacks the initramfs, creates PID 1, and idle loops. PID 1 is **`/init`**, a symlink to `/bin/busybox`; the kernel passes `argv[0]="/init"`, so BusyBox runs its **`init`** applet. That reads `/etc/inittab`, runs `/etc/init.d/rcS`, and respawns a login **`ash`** shell (`-/bin/ash -l`). Environment variables (`PATH`, `HOME`, `TERM`, `PS1`) come from `/etc/profile` when ash starts.

You should see a `~ #` prompt. Try `ls`, `ps`, `cat /etc/inittab`, `halt`, or `poweroff`. Use `reboot -f` to restart (BusyBox calls `reboot(2)` directly; plain `reboot` notifies init via signal).

`make clean` removes objects, `mini-os.elf`, `mini-os.bin`, and the generated initramfs tree (`initramfs/root`).

## Rootfs layout

The cpio image is built under `initramfs/root/`:

```text
/init              → bin/busybox          # kernel exec target; init applet
/sbin/init         → ../bin/busybox       # conventional init path
/bin/busybox       # static BusyBox binary (you supply initramfs/busybox)
/bin/{ash,sh,ls,…} → busybox              # applet symlinks (see Makefile)
/etc/inittab       # BusyBox init config
/etc/init.d/rcS    # early boot hook (no-op by default)
/etc/profile       # login shell environment
/etc/passwd        # minimal root entry
/tmp/
/proc/             # created by kernel proc_init()
/dev/              # directory; /dev/null, /dev/tty, /dev/console are VFS hooks
/bin/hello         # musl syscall/regression test binary
```

There is no musl `/init` stub. The kernel still execs `/init` (initramfs convention); the symlink makes that BusyBox init directly.

## Layout

```text
kernel/     boot, IRQ, scheduler, wait queues, fork/exit, syscalls, signals, reboot, printk
mm/         page allocator, mmap/brk, copy_to/from_user
fs/         ramfs, dcache, path lookup, pipes, procfs, dev hooks, ELF loader
drivers/    UART + console TTY
lib/        kernel string helpers, vsnprintf
include/    linux/, uapi/linux/, and asm/ headers (Linux-shaped, not Linux)
initramfs/  BusyBox rootfs sources, musl test programs (hello)
init/       kernel boot C entry (start_kernel)
```

Headers live under `include/linux`, `include/uapi/linux`, and `include/asm` so files look like kernel code (`current`, `pt_regs`, `__NR_*`) without pulling in the real kernel. Userspace-facing constants such as `WNOHANG` live in `include/uapi/linux/wait.h`; kernel wait-queue types are in `include/linux/wait.h`.

## Main components

### Boot and exceptions

- `kernel/head.S` — EL1 entry, early stack, MMU (identity + high half at `0xffff800080000000`), jump to C. QEMU loads the image at `0x40000000`.
- `kernel/entry.S` — exception vectors, `sync_el0_entry`, `irq_entry`, `switch_to`, `task_trampoline`, `finish_eret`.
- `init/main.c` — `start_kernel()`: UART, timer, TTY, SMP, page allocator, ramfs, unpack initramfs, procfs, scheduler, PID 1.

This is the “CPU trap into the kernel, then `eret` back” story.

### Interrupts and time

- `kernel/irq.c` — GICv2/GICv3 on QEMU virt; timer IRQ.
- `kernel/time/tick.c` — ARM generic timer, jiffies, time slice.

A tick can preempt a user task (`schedule()` from IRQ). Kernel stacks stay per-task so a syscall or IRQ frame survives a context switch.

### Scheduling and tasks

- `include/linux/sched.h` — `task_struct`: pid, tgid, pgid, sid, state, kernel `cpu_context`, user `pt_regs *`, files, cwd, signal mask.
- `kernel/sched/core.c` — runqueue protected by `runqueue_lock`, round-robin `pick_next_task()`, `schedule()` → `switch_to`.
- `kernel/sched/idle.c` — per-CPU idle (PID 0).
- `kernel/sched/wait.c` — wait queues: `prepare_to_wait`, `finish_wait`, `wake_up`.
- `kernel/fork.c` — `kernel_thread()`, `fork` (`clone`), copy page tables and file table.
- `kernel/exit.c` — zombie, `SIGCHLD` to parent, `wait4` (`WNOHANG`, `WUNTRACED`, `WCONTINUED`; interruptible via `-EINTR`).
- `kernel/pid.c` — `getpid`, `getpgrp`, `setpgid`, `getsid`, `setsid`.

States are the usual teaching set: `RUNNING`, `SLEEPING`, `STOPPED`, `ZOMBIE`, idle. There is no CFS, no cgroups, no kernel preemption of kernel threads beyond explicit `schedule()`.

### Synchronization

- `include/linux/spinlock.h` — AArch64 ticketless spinlock via `LDAXR`/`STXR` acquire and `STLR` release; `spin_lock_irqsave` / `spin_unlock_irqrestore` pair with `local_irq_save` / `local_irq_restore` (`include/asm/irqflags.h`).
- `runqueue_lock` in `kernel/sched/core.c` — protects the global runqueue and `schedule()`’s pick-next path; all runqueue walkers take this lock.
- `include/linux/wait.h` + `kernel/sched/wait.c` — Linux-style wait queues for blocking I/O:
  - `DECLARE_WAITQUEUE` on the stack, `prepare_to_wait` → `schedule` → `finish_wait`.
  - `wake_up()` marks sleeping waiters runnable via `wake_up_process()`.
  - Used by `fs/pipe.c` (read/write when the buffer is empty/full) and `drivers/tty/tty.c` (blocking read until UART input arrives).

There are no mutexes, RW locks, or `rcu` — spinlocks plus IRQ masking cover the current SMP-safe paths.

### System calls

`kernel/sys.c` dispatches on `regs->x8` (Linux AArch64 ABI: number in `x8`, args in `x0`–`x5`, return in `x0`). After the call, `do_signal()` runs before returning to EL0.

Implemented (subset):

- I/O and files: `read`, `write`, `writev`, `openat`, `close`, `dup`/`dup3`, `pipe2`, `fstat`, `newfstatat`, `getdents64`, `lseek`, `fcntl` (`F_DUPFD`, `F_DUPFD_CLOEXEC`, `F_GET/SETFD`, `F_GET/SETFL`), `sendfile`
- Paths: `mkdirat`, `unlinkat`, `linkat`, `symlinkat`, `readlinkat`, `chdir`, `getcwd`, `utimensat` (stub)
- Processes: `clone` (always fork), `execve`, `exit` / `exit_group`, `wait4`, `getpid` / `gettid` / `getppid`, `getpgrp`, `setpgid`, `getsid`, `setsid`, `sched_yield`, `set_tid_address`
- Identity / time: `getuid` / `geteuid` / `getgid` / `getegid` (all 0), `uname`, `clock_gettime`, `nanosleep`, `sysinfo`
- Memory: `brk`, `mmap` (anonymous), `munmap`; `mprotect` is a no-op stub (enough for musl CRT)
- Signals: `kill`, `rt_sigaction`, `rt_sigprocmask`, `rt_sigpending`, `rt_sigsuspend`, `rt_sigreturn`
- Reboot: `reboot` (`RESTART`, `HALT`, `POWER_OFF` via PSCI on QEMU virt)
- TTY `ioctl`: `TCGETS`/`TCSETS`, `TIOCGPGRP`/`TIOCSPGRP`, `TIOCSCTTY`, `TIOCGSID`, `TIOCNOTTY`, `TIOCGWINSZ`

Unknown numbers return `-ENOSYS`.

### Signals

`kernel/signal.c` is a small Linux rt-signal path:

- Each task has `pending` and `blocked` bitmasks (signals 1–63).
- `kill` queues a bit (`pid > 0` one task, `pid == 0` caller’s pgrp, `pid < -1` another pgrp, `pid == -1` all user tasks except caller) and wakes a sleeper if the signal is unblocked.
- **`wait4`** is interruptible: if a signal arrives while sleeping, the syscall returns **`-EINTR`**; **`do_signal()`** on the syscall-return path then delivers default actions (e.g. `SIGTERM` → `do_exit()`).
- `do_signal` on syscall return: `SIGSTOP` parks the task in `TASK_STOPPED` (not a zombie); default terminate (except ignored signals like `SIGCHLD` / `SIGCONT`); `SIG_IGN` drop; or user handler.
- PID 1 default `SIGUSR1` / `SIGUSR2` / `SIGTERM` triggers shutdown via `kernel_init_shutdown()` (BusyBox `halt` / `poweroff` path).
- A handler gets a **signal frame** on the user stack; musl’s restorer issues `rt_sigreturn` to restore registers and the old mask.
- `sa_mask` is applied while the handler runs.
- `rt_sigprocmask` / `rt_sigpending` / `rt_sigsuspend` match the Linux “replace mask and sleep until a signal” idea.

`SIGKILL` / `SIGSTOP` cannot be caught or blocked. `SIGCONT` (or `SIGKILL`) resumes a stopped task. `waitpid` with `WUNTRACED` / `WCONTINUED` reports `WIFSTOPPED` / `WIFCONTINUED`.

### Reboot and power off

- `kernel/reboot.c` — `reboot(2)` with Linux magic numbers; `RESTART` / `HALT` / `POWER_OFF`.
- `kernel/psci.c` — PSCI 0.2 `SYSTEM_RESET` / `SYSTEM_OFF` via HVC (QEMU `virt` firmware).
- BusyBox applets `halt`, `poweroff`, and `reboot -f` exercise the `reboot(2)` path; non-`-f` shutdown sends a signal to PID 1, handled in `do_signal()`.

### Virtual memory and ELF

- `mm/page_alloc.c` — physical page pool after the kernel image.
- `mm/mmap.c` — user page tables, `do_map`, `brk`, anonymous `mmap`/`munmap`.
- `mm/uaccess.c` — `copy_to_user` / `copy_from_user` (EL1 access to EL0 mappings).
- `fs/exec.c` / `fs/binfmt.c` — `execve`: load an **AArch64 `ET_EXEC`** ELF, map a user stack, build Linux-style argc/argv/envp/auxv.

User addresses sit in a low range (stack near `0x4040000`, mmap base `0x2000000`). Kernel virtual memory is the high half. Each user task has its own `pgd`; `mm_install()` switches it on context switch.

The initial user stack (SP at the low end, stack grows down):

```text
high: strings, AT_RANDOM
      auxv … AT_NULL
      envp[] NULL
      argv[] NULL
SP  : argc
```

`ET_DYN` (PIE), `PT_INTERP` (dynamic linker), and non-`RELATIVE` relocations are not supported. Static musl programs are built `-static -fno-pie -no-pie`.

The initramfs cpio is linked at the **end** of the kernel image (`linker.ld`) so a larger rootfs does not push `.data.boot` out of `adr` range from `head.S`.

### VFS, ramfs, initramfs

Linux VFS vocabulary, one backing store (ramfs) plus synthetic trees:

- `include/linux/fs.h` — inode, `file`, `file_operations`, `inode_operations`.
- `fs/dcache.c` — dentries, path walk for `.` / `..`, `getcwd`.
- `fs/namei.c` — path resolve, `mkdir`/`unlink`/`link`/`symlink`/`chdir`; final-component symlink follow (depth 8).
- `fs/ramfs.c` — in-memory files and directories, hard links, symlinks (`S_IFLNK`).
- `fs/pipe.c` — pipe buffers and wait queues (`prepare_to_wait` / `wake_up`; `-EINTR` if a signal is pending).
- `fs/open.c`, `fs/read_write.c`, `fs/stat.c`, `fs/readdir.c` — fd table, `fcntl`, `lseek` via `f_op->llseek`.
- `fs/procfs.c` — `/proc`, `/proc/<pid>/stat`, `/proc/<pid>/cmdline` for BusyBox `ps`.
- `fs/dev.c`, `fs/devnull.c`, `fs/devtty.c`, `fs/devconsole.c` — special `/dev/*` path hooks (not real char devices).
- `fs/initramfs.c` — unpack a newc cpio blob into ramfs.

There is no block layer, no ext4, no mount table beyond “everything is ramfs (+ proc + dev hooks).” Shebang (`#!`) execution is not supported — run scripts as `/bin/sh script`.

### Console, printk, and SMP

- `drivers/tty/serial.c` — PL011 UART; `uart_putc` / `uart_puts` / `uart_write` and user `write` to stdout.
- `kernel/printk.c` — `printk()` and Linux-style `pr_info` / `pr_err` / … macros (`include/linux/printk.h`); output goes to UART with `KERN_*` level prefixes.
- `lib/vsnprintf.c` — minimal formatter (`%d`, `%u`, `%x`, `%lx`, `%p`, `%s`, `%c`, `%%`) used by `printk`.
- `drivers/tty/tty.c` — canonical line discipline, echo, job-control signals, termios (`TCGETS`/`TCSETS`), controlling TTY (`TIOCSCTTY`), blocking read on a wait queue, winsize stub.
- `kernel/smp.c` — start secondary CPUs. They print a hello and idle. User tasks currently run on CPU0’s scheduling path.

PID 1 gets fd 0/1/2 on the UART TTY before `kernel_execve("/init")`.

### Userspace

**BusyBox** is the main userspace: static binary at `initramfs/busybox`, copied to `/bin/busybox` with symlinks for common applets (`ash`, `sh`, `ls`, `echo`, `cat`, `sleep`, `ps`, `uname`, `true`, `false`, `pwd`, `halt`, `poweroff`, `reboot` — see `BUSYBOX_APPLETS` in the Makefile).

Boot chain:

1. Kernel → `/init` (busybox symlink, `init` applet).
2. `/etc/inittab` → `sysinit` runs `/bin/sh /etc/init.d/rcS`, then `respawn` starts login ash.
3. `/etc/profile` sets `PATH=/bin:/sbin`, `HOME=/`, `TERM=linux` for interactive shells.

**`/bin/hello`** is a separate musl program (`initramfs/src/hello.c`) used to regression-test syscalls. It is not part of normal boot.

A glibc or dynamically linked userspace will not run. Programs must be static AArch64 `ET_EXEC` ELFs.

## How a syscall looks

1. User `svc #0`.
2. `sync_el0_entry` saves GPRs, ELR, SPSR into `pt_regs` on the kernel stack.
3. `handle_syscall()` runs with IRQs masked around return (so ELR/SPSR are not clobbered).
4. `do_signal()` may change ELR to a handler and push a user stack frame.
5. `finish_eret` restores registers and returns to EL0.

Fork copies that frame onto the child’s kernel stack and points the child at `ret_from_fork`.

## What is deliberately missing

No syscall restart (`SA_RESTART`), no `siginfo`, no `ptrace`, no networking, no disk, no user SMP load balancing. No mutexes or reader/writer locks yet. No PIE loader, no `ld.so`. No real device driver model (`mknod`, block/char dev layers). No shebang interpreter. Many syscalls BusyBox can optionally use are still absent: `faccessat`, `renameat`, `ppoll`, `dup2` (musl usually uses `dup3`), `vhangup`, mount/unmount, etc.

Names like `task_struct` are there so you can grep Linux later and recognize the shape—not so this can merge with Linux.

## Reading order

1. `kernel/head.S` → `init/main.c`
2. `kernel/entry.S` → `kernel/sys.c`
3. `kernel/sched/core.c` → `kernel/sched/wait.c` → `include/linux/spinlock.h` → `kernel/fork.c` → `kernel/exit.c`
4. `fs/ramfs.c` → `fs/dcache.c` → `fs/namei.c`
5. `fs/binfmt.c` → `fs/exec.c`
6. `fs/procfs.c` → `fs/dev.c`
7. `drivers/tty/tty.c` → `kernel/printk.c`
8. `kernel/signal.c` → `kernel/reboot.c` → `kernel/psci.c`
9. `initramfs/etc/inittab`, `initramfs/etc/profile`, and `initramfs/src/hello.c`
