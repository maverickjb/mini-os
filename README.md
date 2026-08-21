# mini-os

A small AArch64 kernel for **learning how Linux-like kernels are put together**. It is not Linux, not POSIX-complete, and not meant for real hardware. It runs on QEMU `virt` and copies Linux *ideas* and *names* so you can follow the same mental model: boot, exceptions, scheduling, processes, virtual memory, a VFS, and system calls.

If you have read kernel source or a textbook chapter on “what a kernel does,” this tree is a walkable, runnable sketch of those pieces.

## What you can see here

| Linux idea | What mini-os does |
| --- | --- |
| Privilege levels | Kernel at EL1, user programs at EL0 |
| Exception vectors | SVC syscalls and IRQs in `vectors.S` |
| Tasks / `task_struct` | Round-robin user and kernel threads |
| Fork / exec / exit / wait | Separate address spaces, ELF load, zombies |
| Signals | Pending bits, `sigaction`, mask, suspend, `sigreturn` |
| Page allocator + user maps | Buddy-style pages, user page tables, `mmap` / `brk` |
| VFS | Inodes, dentries, files, ramfs, pipes |
| Initramfs | cpio image unpacked to ramfs; PID 1 runs `/init` |

Many Linux syscall numbers exist in `include/linux/unistd.h`. Only the ones wired in `kernel/sys.c` actually work.

## Build and run

Needs an AArch64 cross compiler and QEMU:

```text
aarch64-linux-gnu-gcc
qemu-system-aarch64
```

```sh
make
make run
```

QEMU is started as:

```text
qemu-system-aarch64 -machine virt,gic-version=3 -cpu cortex-a72 -smp 4 -nographic -kernel mini-os.elf
```

Boot CPU0 brings up three secondary CPUs, unpacks the initramfs, creates PID 1, and idle loops. PID 1 forks and execs `/bin/hello`, which exercises VFS, processes, and signals.

`make clean` removes objects, `mini-os.elf`, `mini-os.bin`, and the generated initramfs tree.

## Layout

```text
kernel/     boot, IRQ, scheduler, fork/exit, syscalls, signals
mm/         page allocator, mmap/brk, copy_to/from_user
fs/         ramfs, dcache, path lookup, pipes, ELF loader
drivers/    UART console
lib/        kernel string helpers
include/    linux/ and asm/ headers (Linux-shaped, not Linux)
initramfs/  tiny libc + /init and /bin/hello
```

Headers live under `include/linux` and `include/asm` so files look like kernel code (`current`, `pt_regs`, `__NR_*`) without pulling in the real kernel.

## Main components

### Boot and exceptions

- `kernel/head.S` — EL1 entry, early stack, jump to C.
- `mmu_enable.S` — identity and high-half maps so the kernel can run at `0xffff800080000000` while QEMU loads the image at `0x40000000`.
- `vectors.S` — exception vector table. EL0 `svc #0` builds a `pt_regs` frame and calls `syscall_handler`. IRQs save the same frame layout.
- `context.S` — `switch_to` (callee-saved regs + SP) and `finish_eret` (return to EL0).
- `init/main.c` — `start_kernel()`: UART, timer, SMP, page allocator, ramfs, unpack initramfs, scheduler, PID 1.

This is the “CPU trap into the kernel, then `eret` back” story.

### Interrupts and time

- `kernel/irq.c` — GICv2/GICv3 on QEMU virt; timer IRQ.
- `kernel/time/tick.c` — ARM generic timer, jiffies, time slice.

A tick can preempt a user task (`schedule()` from IRQ). Kernel stacks stay per-task so a syscall or IRQ frame survives a context switch.

### Scheduling and tasks

- `include/linux/sched.h` — `task_struct`: pid, state, kernel `cpu_context`, user `pt_regs *`, files, cwd, signal mask.
- `kernel/sched/core.c` — runqueue, round-robin `pick_next_task()`, `schedule()` → `switch_to`.
- `kernel/sched/idle.c` — per-CPU idle (PID 0).
- `kernel/fork.c` — `kernel_thread()`, `fork` (`clone`), copy page tables and file table.
- `kernel/exit.c` — zombie, `SIGCHLD` to parent, `wait4` (`WNOHANG`, `WUNTRACED`, `WCONTINUED`).
- `kernel/pid.c` — `getpid`, `getpgrp`.

States are the usual teaching set: `RUNNING`, `SLEEPING`, `STOPPED`, `ZOMBIE`, idle. There is no CFS, no cgroups, no kernel preemption of kernel threads beyond explicit `schedule()`.

### System calls

`kernel/sys.c` dispatches on `regs->x8` (Linux AArch64 ABI: number in `x8`, args in `x0`–`x5`, return in `x0`). After the call, `do_signal()` runs before returning to EL0.

Implemented (subset):

- I/O and files: `read`, `write`, `openat`, `close`, `dup`/`dup3`, `pipe2`, `fstat`, `newfstatat`, `getdents64`
- Paths: `mkdirat`, `unlinkat`, `linkat`, `chdir`, `getcwd`
- Processes: `clone` (fork), `execve`, `exit`, `wait4`, `getpid`, `getpgrp`, `sched_yield`
- Memory: `brk`, `mmap`, `munmap`
- Signals: `kill`, `rt_sigaction`, `rt_sigprocmask`, `rt_sigpending`, `rt_sigsuspend`, `rt_sigreturn`

Unknown numbers return `-ENOSYS`.

### Signals

`kernel/signal.c` is a small Linux rt-signal path:

- Each task has `pending` and `blocked` bitmasks (signals 1–63).
- `kill` queues a bit and wakes a sleeper if the signal is unblocked.
- `do_signal` on syscall return: `SIGSTOP` parks the task in `TASK_STOPPED` (not a zombie); default terminate (except ignored signals like `SIGCHLD` / `SIGCONT`); `SIG_IGN` drop; or user handler.
- A handler gets a **signal frame** on the user stack; libc `__restore_rt` issues `rt_sigreturn` to restore registers and the old mask.
- `sa_mask` is applied while the handler runs.
- `rt_sigprocmask` / `rt_sigpending` / `rt_sigsuspend` match the Linux “replace mask and sleep until a signal” idea.

`SIGKILL` / `SIGSTOP` cannot be caught or blocked. `SIGCONT` (or `SIGKILL`) resumes a stopped task. `waitpid` with `WUNTRACED` / `WCONTINUED` reports `WIFSTOPPED` / `WIFCONTINUED`.

### Virtual memory

- `mm/page_alloc.c` — physical page pool after the kernel image.
- `mm/mmap.c` — user page tables, `do_map`, `brk`, anonymous `mmap`/`munmap`.
- `mm/uaccess.c` — `copy_to_user` / `copy_from_user` (EL1 access to EL0 mappings).
- `fs/binfmt.c` — load an ELF into a new `mm_struct`, map a user stack.

User addresses sit in a low range (stack near `0x4040000`, mmap base `0x2000000`). Kernel virtual memory is the high half. Each user task has its own `pgd`; `mm_install()` switches it on context switch.

### VFS, ramfs, initramfs

Linux VFS vocabulary, one filesystem:

- `include/linux/fs.h` — inode, `file`, `file_operations`, `inode_operations`.
- `fs/dcache.c` — dentries, path walk for `.` / `..`, `getcwd`.
- `fs/namei.c` — path resolve, `mkdir`/`unlink`/`link`/`chdir`.
- `fs/ramfs.c` — in-memory files and directories, `nlink` hard links.
- `fs/pipe.c` — pipe buffers and wait queues (`-EINTR` if a signal is pending).
- `fs/open.c`, `fs/read_write.c`, `fs/stat.c`, `fs/readdir.c` — fd table helpers.
- `fs/initramfs.c` — unpack a newc cpio blob into ramfs.
- `fs/exec.c` / `fs/binfmt.c` — `execve`.

There is no block layer, no ext4, no mount table beyond “everything is ramfs.”

### Console and SMP

- `drivers/tty/serial.c` — PL011 UART; kernel `uart_puts` and user `write` to stdout.
- `kernel/smp.c` — start secondary CPUs. They print a hello and idle. User tasks currently run on CPU0’s scheduling path.

### Userspace

`initramfs/` is a tiny freestanding libc (syscalls, `printf`, `malloc`, signals) plus:

- `/init` — PID 1: list root, fork/exec `/bin/hello`, `waitpid`.
- `/bin/hello` — mkdir, getdents, link/unlink, pipes, kill, sigaction, sigreturn, masks, sigpending, sigsuspend, `WNOHANG`.

Programs are linked `-nostdlib` with `crt0.S`; they are ordinary EL0 ELFs, not kernel modules.

## How a syscall looks

1. User `svc #0`.
2. `sync_el0_entry` saves GPRs, ELR, SPSR into `pt_regs` on the kernel stack.
3. `handle_syscall()` runs with IRQs masked around return (so ELR/SPSR are not clobbered).
4. `do_signal()` may change ELR to a handler and push a user stack frame.
5. `finish_eret` restores registers and returns to EL0.

Fork copies that frame onto the child’s kernel stack and points the child at `ret_from_fork`.

## What is deliberately missing

No syscall restart (`SA_RESTART`), no `siginfo`, no process groups, no `ptrace`, no networking, no disk, no user SMP load balancing, no locking beyond “IRQs off.” Names like `task_struct` are there so you can grep Linux later and recognize the shape—not so this can merge with Linux.

## Reading order

1. `kernel/head.S` → `init/main.c`
2. `vectors.S` → `kernel/sys.c`
3. `kernel/sched/core.c` → `kernel/fork.c` → `kernel/exit.c`
4. `fs/ramfs.c` → `fs/dcache.c` → `fs/namei.c`
5. `kernel/signal.c`
6. `initramfs/src/init.c` and `initramfs/src/hello.c`
