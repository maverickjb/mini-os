# mini-os

A small AArch64 kernel for **learning how Linux-like kernels are put together**. It is not Linux, not POSIX-complete, and not meant for real hardware. It runs on QEMU `virt` and copies Linux *ideas* and *names* so you can follow the same mental model: boot, exceptions, scheduling, processes, virtual memory, a VFS, and system calls.

If you have read kernel source or a textbook chapter on “what a kernel does,” this tree is a walkable, runnable sketch of those pieces.

## What you can see here

| Linux idea | What mini-os does |
| --- | --- |
| Privilege levels | Kernel at EL1, user programs at EL0 |
| Exception vectors | SVC syscalls and IRQs in `kernel/entry.S` |
| Tasks / `task_struct` | Round-robin user and kernel threads on per-CPU runqueues |
| SMP | 4 CPUs on QEMU virt; PSCI bring-up; reschedule IPI; pull load balance |
| Fork / exec / exit / wait | Separate address spaces, ELF load, zombies |
| Process groups / sessions | `pgid` / `sid`, `setpgid`, `setsid`, TTY foreground pgrp |
| Signals | Pending bits, `sigaction`, mask, suspend, `sigreturn` |
| Page allocator + user maps | Buddy-style pages, user page tables, VMA list, `mmap` / `brk` / `munmap` |
| SLUB / `kmalloc` | Per-size object caches (32–2048 B); large allocs via buddy pages; kernel objects (tasks, files, dentries, mm, pipes, proc inodes, ramfs nodes, VMAs) |
| VFS | Inodes, dentries, files, ramfs, pipes, symlinks |
| `/proc` | Minimal procfs for `ps` (`/proc/<pid>/stat`, `cmdline`) |
| Device nodes | Path hooks for `/dev/null`, `/dev/tty`, `/dev/console` |
| Initramfs + BusyBox | cpio rootfs; PID 1 is BusyBox `init` via `/init` → `busybox` |
| Kernel logging | `printk` / `pr_*` → UART; minimal `vsnprintf` |
| Synchronization | AArch64 spinlocks; wait queues; `wait_event` helpers |
| Kernel data structures | Doubly-linked lists, red-black tree (`list.h`, `rbtree`) |
| Userspace kselftests | `tools/testing/selftests/` (TAP); in-kernel KUnit-style tests TBD |

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

Boot CPU0 brings up three secondary CPUs (per-CPU idle, timer, runqueue), unpacks the initramfs, creates PID 1, and idle loops. PID 1 is **`/init`**, a symlink to `/bin/busybox`; the kernel passes `argv[0]="/init"`, so BusyBox runs its **`init`** applet. That reads `/etc/inittab`, runs `/etc/init.d/rcS`, and respawns a login **`ash`** shell (`-/bin/ash -l`). Environment variables (`PATH`, `HOME`, `TERM`, `PS1`) come from `/etc/profile` when ash starts. User tasks may be pulled onto secondary CPUs by the idle load balancer.

You should see a `~ #` prompt. Try `ls`, `ps`, `cat /etc/inittab`, `halt`, or `poweroff`. Use `reboot -f` to restart (BusyBox calls `reboot(2)` directly; plain `reboot` notifies init via signal). Userspace kselftests live under `/kselftests/` (see below).

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
tools/testing/selftests/   userspace kselftests (Linux-shaped TAP)
kernel/     boot, IRQ, SMP, scheduler, wait queues, fork/exit, syscalls, signals, reboot, printk
mm/         buddy page allocator, SLUB (`kmalloc`), VMA list + mmap/brk/munmap, copy_to/from_user
fs/         ramfs, dcache, path lookup, pipes, procfs, dev hooks, ELF loader
drivers/    UART + console TTY (SMP-safe locks)
lib/        string helpers, vsnprintf, red-black tree
include/    linux/, uapi/linux/, and asm/ headers (Linux-shaped, not Linux)
initramfs/  BusyBox rootfs sources, musl test programs (hello)
init/       kernel boot C entry (start_kernel)
```

Headers live under `include/linux`, `include/uapi/linux`, and `include/asm` so files look like kernel code (`current`, `pt_regs`, `__NR_*`) without pulling in the real kernel. Userspace-facing constants such as `WNOHANG` live in `include/uapi/linux/wait.h`; kernel wait-queue types are in `include/linux/wait.h`.

## Main components

### Boot and exceptions

- `kernel/head.S` — EL1 entry, early stack, MMU (identity + high half at `0xffff800080000000`), jump to C. QEMU loads the image at `0x40000000`.
- `kernel/entry.S` — exception vectors, `sync_el0_entry`, `irq_entry`, `switch_to`, `task_trampoline`, `finish_eret`.
- `init/main.c` — `start_kernel()`: UART, timer, TTY, SMP, page allocator, SLUB, `mmap_init`, ramfs, unpack initramfs, procfs, scheduler, PID 1.

This is the “CPU trap into the kernel, then `eret` back” story.

### Interrupts and time

- `kernel/irq.c` — GICv3 on QEMU virt; per-CPU timer PPI; SGI for reschedule IPI (`IPI_RESCHEDULE`).
- `kernel/time/tick.c` — ARM generic timer on each CPU; `jiffies` / sleeper wakeups / UART RX poll stay on **CPU0** so SMP does not advance time N×.
- **`need_resched`** — the tick (and reschedule IPI) only set a flag on the current task. `irq_exit()` calls `schedule()` when `need_resched && interrupted_el0`. Idle CPUs still schedule from their WFI loop after an IPI wake.

A tick can preempt a user task without calling `schedule()` from the IRQ handler itself. Kernel stacks stay per-task so a syscall or IRQ frame survives a context switch.

### Scheduling and tasks

- `include/linux/sched.h` — `task_struct`: pid, tgid, pgid, sid, state, `cpu`, `need_resched`, kernel `cpu_context`, user `pt_regs *`, embedded `run_list` / `task_list`, files, cwd, signal mask.
- `struct rq` — per-CPU runqueue: `lock`, `tasks`, `nr_running`, `cpu`. Embedded in `cpu_data[cpu].rq` (`include/asm/smp.h`).
- **`current`** — `TPIDR_EL1` holds `&cpu_data[this_cpu]`; `this_cpu_ptr()->curr` is the running task (also used from `prepare_kstack_el0` in asm).
- `all_tasks` — global task list (sleeping, stopped, zombie, runnable); protected by **`tasklist_lock`**, separate from each `rq.lock`.
- `kernel/sched/core.c` — round-robin `pick_next_task()`, `enqueue_task` / `enqueue_task_cpu` / `dequeue_task`, `migrate_task()`, `resched_cpu()`, pull-based `try_pull_task()`, `sched_block()`, `schedule()` → `switch_to`.
- `kernel/sched/idle.c` — per-CPU idle (PID 0); IRQs enabled in `cpu_idle()` so secondaries take timer/IPI.
- `kernel/sched/wait.c` — wait queues: `prepare_to_wait`, `finish_wait`, `wake_up`, `wait_event`, `wait_event_interruptible`.
- `kernel/fork.c` — `kernel_thread()`, `fork` (`clone`), copy page tables, VMA list, and file table; `task_attach()` on creation; `wake_up_process()` enqueues then `resched_cpu(task->cpu)`. `task_struct` and `mm_struct` allocated with `kmalloc`.
- `kernel/exit.c` — zombie, `SIGCHLD` to parent, `wait4` (`WNOHANG`, `WUNTRACED`, `WCONTINUED`; interruptible via `-EINTR`); `task_detach()` on reap; `kfree(task)` after stack and page tables are released.
- `kernel/pid.c` — `getpid`, `getpgrp`, `setpgid`, `getsid`, `setsid`.

**Runqueue policy:** only `TASK_RUNNING` tasks sit on a CPU’s `rq.tasks`. Sleep/stop/exit calls `sched_block()` → `dequeue_task()`; wake paths call `wake_up_process()` → `enqueue_task()` (or `enqueue_task_cpu`) and may IPI the target CPU. Non-runnable tasks remain on `all_tasks` for `wait4`, signals, and `/proc` walks.

**Load balance:** when `schedule()` would run idle on an empty local rq, `try_pull_task()` steals one runnable task from a busier CPU (`source->nr_running > dest->nr_running + 1`), never the source’s `curr` and never its last task. User tasks may run on any online CPU (TTBR0 / UAO set up on secondaries).

States are the usual teaching set: `RUNNING`, `SLEEPING`, `STOPPED`, `ZOMBIE`, idle. There is no CFS, no cgroups, no kernel preemption of kernel threads beyond explicit `schedule()`.

### Synchronization

- `include/linux/spinlock.h` — AArch64 ticketless spinlock via `LDAXR`/`STXR` acquire and `STLR` release; `spin_lock_irqsave` / `spin_unlock_irqrestore` pair with `local_irq_save` / `local_irq_restore` (`include/asm/irqflags.h`).
- **`rq.lock`** — protects that CPU’s runnable list (`tasks`, `nr_running`, `pick_next_task`, `schedule()`). `migrate_task()` locks two rqs in CPU-id order.
- **`tasklist_lock`** — protects the global `all_tasks` list (`task_attach`, `task_detach`, `for_each_task` walkers). Kept separate from `rq.lock` so runqueue and task-enumeration locking do not alias.
- **`uart_lock` / `tty0.lock`** — serialize PL011 MMIO and the console TTY RX ring / termios (see Console below).
- `include/linux/wait.h` + `kernel/sched/wait.c` — Linux-style wait queues for blocking I/O:
  - `DECLARE_WAITQUEUE` on the stack, `prepare_to_wait` → `schedule` → `finish_wait`.
  - `wait_event(wq, condition)` — sleep until a function-pointer condition is true.
  - `wait_event_interruptible(wq, condition)` — same, but returns `-EINTR` if a signal is pending.
  - `wake_up()` marks sleeping waiters runnable via `wake_up_process()`.
  - Used by `fs/pipe.c` (read/write when the buffer is empty/full) and `drivers/tty/tty.c` (`wait_event_interruptible` on blocking read).
- `include/linux/list.h` — doubly-linked `list_head` helpers (`list_add`, `list_del_init`, `list_is_linked`, `list_for_each`). Off-list nodes are self-linked (`INIT_LIST_HEAD` / `list_del_init`); never use bare `list_del`.
- `include/linux/rbtree.h` + `lib/rbtree.c` — minimal red-black tree (insert, erase, in-order walk).

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
- A handler gets a **signal frame** on the user stack (accepted if SP lies in a writable stack VMA, else the full `USER_STACK_SIZE` window); musl’s restorer issues `rt_sigreturn` to restore registers and the old mask.
- `sa_mask` is applied while the handler runs.
- `rt_sigprocmask` / `rt_sigpending` / `rt_sigsuspend` match the Linux “replace mask and sleep until a signal” idea.

`SIGKILL` / `SIGSTOP` cannot be caught or blocked. `SIGCONT` (or `SIGKILL`) resumes a stopped task. `waitpid` with `WUNTRACED` / `WCONTINUED` reports `WIFSTOPPED` / `WIFCONTINUED`.

### Reboot and power off

- `kernel/reboot.c` — `reboot(2)` with Linux magic numbers; `RESTART` / `HALT` / `POWER_OFF`.
- `kernel/psci.c` — PSCI 0.2 client: `psci_cpu_on`, `psci_system_reset`, `psci_system_off` (HVC on QEMU `virt`).
- BusyBox applets `halt`, `poweroff`, and `reboot -f` exercise the `reboot(2)` path; non-`-f` shutdown sends a signal to PID 1, handled in `do_signal()`.

### Virtual memory, SLUB, and ELF

- `mm/page_alloc.c` — buddy allocator for physical pages after the kernel image (`alloc_pages` / `free_pages`).
- `mm/slub.c` — SLUB-style `kmalloc` / `kfree`:
  - Fixed-size caches: 32, 64, 128, 256, 512, 1024, 2048 bytes.
  - Each page starts with a `struct slab` header; free objects linked through an embedded freelist.
  - Partial slabs kept on a per-cache list; empty slabs returned to the buddy allocator.
  - Allocations larger than 2048 bytes use whole buddy pages (slab header stores page order).
  - Dedicated caches via static `struct kmem_cache` + `kmem_cache_init` (no dynamic `kmem_cache_create`).
  - `slub_init()` runs after `page_alloc_init()`.
- **What uses `kmalloc` / SLUB:** `task_struct` (large alloc — struct exceeds 2048 B), `mm_struct`, `struct file`, VFS `dentry`, `struct pipe`, `struct proc_inode`, ramfs nodes / file data, and `vm_area_struct` (dedicated `vma_cache` via `kmem_cache_alloc`).
- **What still uses `alloc_pages`:** kernel stacks (`order` 1), page-table nodes (`pgd` and copied tables), user mapping pages (`mmap`/`brk`/ELF load), and SLUB slab backing pages.
- `mm/mmap.c` — user address spaces:
  - **`struct vm_area_struct`** — sorted singly-linked list on `mm->mmap` (`vm_start` / `vm_end` exclusive / `vm_flags`).
  - `mmap_init()` — creates the VMA SLUB cache (after `slub_init()`).
  - Helpers: `find_vma`, `vma_record`, `vma_erase_range` (trim / split / remove; split failure returns `-ENOMEM`), `dup_vma_list`, `free_all_vmas`.
  - `do_map` / page-table walk — map anonymous pages into TTBR0 tables.
  - `do_brk` / `do_mmap` / `do_munmap` — grow or shrink the heap VMA, allocate anonymous regions (`MAP_ANONYMOUS`, optional `MAP_FIXED`), and unmap pages while keeping the VMA list consistent.
  - `mm_alloc` / `dup_mm` / `mm_put` — allocate or duplicate `mm_struct` (including VMA list + full page-table copy); last user frees tables, VMAs, then `kfree(mm)`.
- `mm/uaccess.c` — `copy_to_user` / `copy_from_user` (EL1 access to EL0 mappings).
- `fs/exec.c` / `fs/binfmt.c` — `execve`: load an **AArch64 `ET_EXEC`** ELF, map a user stack, `vma_record` each `PT_LOAD` and the stack, build Linux-style argc/argv/envp/auxv, then drop the old `mm`.

User addresses sit in a low range (stack near `0x4040000`, mmap base `0x2000000`). Kernel virtual memory is the high half. Each user task has its own `pgd`; `mm_install()` switches it on context switch. Fork still fully copies page tables (`dup_pgtable`); there is no COW or shared-mm `vfork` yet.

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
- `fs/dcache.c` — dentries (`kmalloc`), path walk for `.` / `..`, `getcwd`.
- `fs/namei.c` — path resolve, `mkdir`/`unlink`/`link`/`symlink`/`chdir`; final-component symlink follow (depth 8).
- `fs/ramfs.c` — in-memory files and directories, hard links, symlinks (`S_IFLNK`); nodes and data buffers via `kmalloc`.
- `fs/pipe.c` — anonymous pipes (`kmalloc` for `struct pipe`); wait queues (`prepare_to_wait` / `wake_up`; `-EINTR` if a signal is pending).
- `fs/open.c`, `fs/read_write.c`, `fs/stat.c`, `fs/readdir.c` — fd table (`alloc_file()` → `kmalloc`), `fcntl`, `lseek` via `f_op->llseek`.
- `fs/procfs.c` — `/proc`, `/proc/<pid>/stat`, `/proc/<pid>/cmdline` for BusyBox `ps` (`proc_inode` via `kmalloc`).
- `fs/dev.c`, `fs/devnull.c`, `fs/devtty.c`, `fs/devconsole.c` — special `/dev/*` path hooks (not real char devices).
- `fs/initramfs.c` — unpack a newc cpio blob into ramfs.

There is no block layer, no ext4, no mount table beyond “everything is ramfs (+ proc + dev hooks).” Shebang (`#!`) execution is not supported — run scripts as `/bin/sh script`.

### Console, printk, and SMP

- `drivers/tty/serial.c` — PL011 UART; **`uart_lock`** serializes all MMIO so concurrent `printk` / `tty_write` / RX on different CPUs cannot corrupt TX or wedge the FIFO. `uart_write` / `serial_write_n` hold the lock for a whole string so lines stay intact. RX IRQ drops the lock before `tty_receive_char()` so echo cannot deadlock.
- `kernel/printk.c` — `printk()` and Linux-style `pr_info` / `pr_err` / … macros (`include/linux/printk.h`); output goes to UART with `KERN_*` level prefixes.
- `lib/vsnprintf.c` — minimal formatter (`%d`, `%u`, `%x`, `%lx`, `%p`, `%s`, `%c`, `%%`) used by `printk`.
- `drivers/tty/tty.c` — canonical line discipline, echo, job-control signals, termios (`TCGETS`/`TCSETS`), controlling TTY (`TIOCSCTTY`), blocking read via `wait_event_interruptible`, winsize stub. **`tty0.lock`** protects the RX ring, termios flags, and session/pgrp against readers and `serial_irq` on another CPU; echo calls `serial_putc` outside that lock.
- `kernel/smp.c` — bring up secondary CPUs with `psci_cpu_on`; each gets its own idle task, runqueue, timer, and `TPIDR_EL1`. `send_reschedule_ipi` / `handle_reschedule_ipi` set `need_resched` on the target CPU after migration or wake.

PID 1 gets fd 0/1/2 on the UART TTY before `kernel_execve("/init")`.

### Userspace

**BusyBox** is the main userspace: static binary at `initramfs/busybox`, copied to `/bin/busybox` with symlinks for common applets (`ash`, `sh`, `ls`, `echo`, `cat`, `sleep`, `ps`, `uname`, `true`, `false`, `pwd`, `halt`, `poweroff`, `reboot` — see `BUSYBOX_APPLETS` in the Makefile).

Boot chain:

1. Kernel → `/init` (busybox symlink, `init` applet).
2. `/etc/inittab` → `sysinit` runs `/bin/sh /etc/init.d/rcS`, then `respawn` starts login ash.
3. `/etc/profile` sets `PATH=/bin:/sbin`, `HOME=/`, `TERM=linux` for interactive shells.

**`/bin/hello`** is a separate musl program (`initramfs/src/hello.c`) used to regression-test syscalls. It is not part of normal boot.

A glibc or dynamically linked userspace will not run. Programs must be static AArch64 `ET_EXEC` ELFs.

### Userspace kselftests

Linux-shaped tree under `tools/testing/selftests/` (header + `lib.mk` + per-suite dirs + `run_kselftest.sh`). Built with the musl toolchain and installed into the initramfs as `/kselftests/`. In-kernel KUnit-style tests are not present yet.

```sh
make kselftest          # build + install into initramfs/root/kselftests
# after boot (no shebang exec yet — invoke via ash):
sh /kselftests/run_kselftest.sh
/kselftests/yield/yield_test
```

- `kselftest.h` — TAP helpers (`ksft_print_header`, `ksft_set_plan`, `ksft_test_result`, `ksft_finished`), same usage pattern as Linux.
- `yield/yield_test.c` — sample suite: `sched_yield` returns 0.

To add a suite: create `tools/testing/selftests/foo/`, add a `Makefile` with `TEST_GEN_PROGS` and `include ../lib.mk`, then append `foo` to `TARGETS` in `tools/testing/selftests/Makefile`.

## How a syscall looks

1. User `svc #0`.
2. `sync_el0_entry` saves GPRs, ELR, SPSR into `pt_regs` on the kernel stack.
3. `handle_syscall()` runs with IRQs masked around return (so ELR/SPSR are not clobbered).
4. `do_signal()` may change ELR to a handler and push a user stack frame.
5. `finish_eret` restores registers and returns to EL0.

Fork copies that frame onto the child’s kernel stack and points the child at `ret_from_fork`.

## What is deliberately missing

No syscall restart (`SA_RESTART`), no `siginfo`, no `ptrace`, no networking, no disk. No mutexes or reader/writer locks yet. No CFS / push balancing (only idle pull). No PIE loader, no `ld.so`. No file-backed `mmap`, no COW / shared page tables on fork. No real device driver model (`mknod`, block/char dev layers). No shebang interpreter. Many syscalls BusyBox can optionally use are still absent: `faccessat`, `renameat`, `ppoll`, `dup2` (musl usually uses `dup3`), `vhangup`, mount/unmount, etc.

Names like `task_struct` are there so you can grep Linux later and recognize the shape—not so this can merge with Linux.

## Reading order

1. `kernel/head.S` → `init/main.c`
2. `kernel/entry.S` → `kernel/sys.c`
3. `kernel/smp.c` → `include/asm/smp.h` → `kernel/irq.c` → `kernel/time/tick.c`
4. `kernel/sched/core.c` → `kernel/sched/wait.c` → `include/linux/spinlock.h` → `mm/slub.c` → `kernel/fork.c` → `kernel/exit.c`
5. `include/linux/list.h` → `include/linux/rbtree.h` → `lib/rbtree.c`
6. `include/linux/mm_types.h` → `mm/mmap.c` (VMAs, `do_brk` / `do_mmap` / `do_munmap`)
7. `fs/ramfs.c` → `fs/dcache.c` → `fs/namei.c`
8. `fs/binfmt.c` → `fs/exec.c`
9. `fs/procfs.c` → `fs/dev.c`
10. `drivers/tty/serial.c` → `drivers/tty/tty.c` → `kernel/printk.c`
11. `kernel/signal.c` → `kernel/reboot.c` → `kernel/psci.c`
12. `tools/testing/selftests/` (userspace TAP) and `initramfs/src/hello.c`
13. `initramfs/etc/inittab`, `initramfs/etc/profile`
