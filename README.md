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
| Signals | Pending bits, `sigaction`, mask, suspend, `sigreturn`, `SA_RESTART` |
| Page allocator + user maps | Buddy + `struct page`; fork COW; `mprotect` updates VMAs and PTEs |
| SLUB / `kmalloc` | Per-size object caches (32–2048 B); large allocs via buddy pages; kernel objects (tasks, files, dentries, mm, pipes, proc inodes, ramfs nodes, VMAs) |
| VFS | Inodes, dentries, files, ramfs, pipes, symlinks |
| `/proc` | Minimal procfs for `ps` (`/proc/<pid>/stat`, `cmdline`) and `/proc/schedstat` |
| Device nodes | `mknod` + `register_chrdev`; `/dev/null`, `/dev/tty`, `/dev/console` |
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
qemu-system-aarch64 -machine virt,gic-version=3 -cpu cortex-a72 -smp 4 -nographic \
    -netdev user,id=n0 -device virtio-net-device,netdev=n0 -kernel mini-os.elf
```

Boot CPU0 brings up three secondary CPUs (per-CPU idle, timer, runqueue), unpacks the initramfs, creates PID 1, and idle loops. PID 1 is **`/init`**, a symlink to `/bin/busybox`; the kernel passes `argv[0]="/init"`, so BusyBox runs its **`init`** applet. That reads `/etc/inittab`, runs `/etc/init.d/rcS`, and respawns a login **`ash`** shell (`-/bin/ash -l`). Environment variables (`PATH`, `HOME`, `TERM`, `PS1`) come from `/etc/profile` when ash starts. User tasks may be pulled onto secondary CPUs by the idle load balancer.

You should see a `~ #` prompt. Try `ls`, `ps`, `date`, `cat /etc/inittab`, `halt`, or `poweroff`. Use `reboot -f` to restart (BusyBox calls `reboot(2)` directly; plain `reboot` notifies init via signal). Userspace kselftests live under `/kselftests/` (see below).

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
/etc/TZ            # POSIX timezone (`CST-8` = UTC+8); `date` uses `TZ`, kernel clock stays UTC
/etc/passwd        # minimal root entry
/tmp/
/proc/             # created by kernel proc_init()
/dev/              # char nodes (null, tty, console) via mknod + chrdev
/kselftests/       # TAP userspace tests (yield, mm, files, signals, time, sched)
/bin/hello         # musl syscall/regression test binary
```

There is no musl `/init` stub. The kernel still execs `/init` (initramfs convention); the symlink makes that BusyBox init directly.

## Layout

```text
tools/testing/selftests/   userspace kselftests (Linux-shaped TAP)
hypervisor/ EL2 boots mini-OS guest @ 0x40000000 (hyp @ 0x44000000)
kernel/     boot, IRQ, SMP, scheduler, wait queues, fork/exit, syscalls, signals, reboot, printk, timekeeping
mm/         buddy page allocator, SLUB (`kmalloc`), VMA list + mmap/brk/munmap/mprotect, COW, copy_to/from_user
fs/         ramfs, dcache, path lookup, pipes, procfs, chrdev + `/dev` nodes, ELF loader
drivers/    UART + console TTY (SMP-safe locks); PL031 RTC; virtio-mmio probe
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
- `init/main.c` — `start_kernel()`: UART, timer, TTY, virtio-mmio scan, SMP, page allocator, SLUB, `mmap_init`, ramfs, unpack initramfs, procfs, `dev_init`, scheduler, PID 1.

This is the “CPU trap into the kernel, then `eret` back” story.

### Interrupts and time

- `kernel/irq.c` — GICv3 on QEMU virt; per-CPU timer PPI; SGI for reschedule IPI (`IPI_RESCHEDULE`).
- `kernel/time/tick.c` — ARM generic timer on each CPU; `jiffies` / sleeper wakeups / UART RX poll stay on **CPU0** so SMP does not advance time N×.
- `kernel/time/time.c` / `clocksource.c` — `get_real_time()` / `set_real_time()`: PL031 Unix seconds plus CNTPCT sub-second offset. `clock_gettime` / `clock_settime` (`CLOCK_REALTIME`) use this; `CLOCK_MONOTONIC` stays on `jiffies` (set is `-EINVAL`).
- `drivers/rtc/pl031.c` — QEMU virt PL031 RTC at `0x09010000` (read `RTCDR`, write `RTCLR`).
- `drivers/virtio/virtio_mmio.c` — scan QEMU virt virtio-mmio slots (`0x0a000000`, 32 × `0x200`). For net: reset, `ACKNOWLEDGE|DRIVER`, print features, queue sizes, MAC. QEMU 8’s transport is often **mmio v1** on the last slot. No virtqueues yet. `make run` attaches `virtio-net-device`.
- **`need_resched`** — the tick (and reschedule IPI) set a flag on the target CPU’s `curr`. `irq_exit()` calls `schedule()` when `need_resched && interrupted_el0` (user preemption only). Idle sets `TIF_POLLING_NRFLAG` on **that CPU’s idle** `task_struct` while watching `need_resched`, then clears it before `WFI`; a remote wake IPIs only if the idle task is not polling.

A tick can preempt a user task without calling `schedule()` from the IRQ handler itself. Kernel stacks stay per-task so a syscall or IRQ frame survives a context switch.

### Scheduling and tasks

- `include/linux/sched.h` — `task_struct`: pid, tgid, pgid, sid, state, `cpu`, `need_resched`, `flags` (`TIF_POLLING_NRFLAG` on idle), kernel `cpu_context`, user `pt_regs *`, embedded `run_list` / `task_list`, files, cwd, signal mask.
- `struct rq` — per-CPU runqueue: `lock`, `tasks`, `nr_running`, `cpu`. Embedded in `cpu_data[cpu].rq` (`include/asm/smp.h`).
- **`current`** — `TPIDR_EL1` holds `&cpu_data[this_cpu]`; `this_cpu_ptr()->curr` is the running task (also used from `prepare_kstack_el0` in asm).
- `all_tasks` — global task list (sleeping, stopped, zombie, runnable); protected by **`tasklist_lock`**, separate from each `rq.lock`.
- `kernel/sched/core.c` — round-robin `pick_next_task()`, `enqueue_task` / `enqueue_task_cpu` / `dequeue_task`, `migrate_task()`, `resched_cpu()`, pull-based `try_pull_task()`, `sched_block()`, `schedule()` → `switch_to`.
- `kernel/sched/idle.c` — per-CPU idle (PID 0); poll `need_resched` with `TIF_POLLING_NRFLAG` on the idle task, then `WFI` (IRQs off). Wake sets `curr->need_resched` and IPIs only if that idle task is not polling.
- `kernel/sched/wait.c` — wait queues: `prepare_to_wait`, `finish_wait`, `wake_up`, `wait_event`, `wait_event_interruptible`.
- `kernel/fork.c` — `kernel_thread()`, `fork` (`clone`), copy page tables, VMA list, and file table; `task_attach()` on creation; `wake_up_process()` enqueues on the lightest (or idle) CPU then `resched_cpu(task->cpu)`. `task_struct` and `mm_struct` allocated with `kmalloc`.
- `kernel/exit.c` — zombie, `SIGCHLD` to parent, `wait4` (`WNOHANG`, `WUNTRACED`, `WCONTINUED`; interruptible via `-ERESTARTSYS` / `SA_RESTART`); `task_detach()` on reap; `kfree(task)` after stack and page tables are released.
- `kernel/pid.c` — `getpid`, `getpgrp`, `setpgid`, `getsid`, `setsid`.

**Runqueue policy:** only `TASK_RUNNING` tasks sit on a CPU’s `rq.tasks`. Sleep/stop/exit calls `sched_block()` → `dequeue_task()`; wake paths call `wake_up_process()` → `enqueue_task()` (or `enqueue_task_cpu`) and may IPI the target CPU. Non-runnable tasks remain on `all_tasks` for `wait4`, signals, and `/proc` walks.

**Load balance:** new tasks (`fork`, first wake) pick the online CPU with the smallest `rq.nr_running`. A later wake may move a task onto an idle CPU if its previous CPU is already busy. Each `schedule()` may `try_pull_task()`: steal one runnable task from a busier CPU if `source->nr_running > dest->nr_running + 1`, never the source’s `curr` and never its last task. Idle-only pull would stop at 5+1+1+1 after one steal; this keeps pulling while underloaded so 8 hogs can reach 2+2+2+2. User tasks may run on any online CPU (TTBR0 / UAO set up on secondaries).

States are the usual teaching set: `RUNNING`, `SLEEPING`, `STOPPED`, `ZOMBIE`, idle. There is no CFS, no cgroups, no kernel preemption of kernel threads beyond explicit `schedule()`.

### Synchronization

- `include/linux/spinlock.h` — AArch64 ticketless spinlock via `LDAXR`/`STXR` acquire and `STLR` release; `spin_lock_irqsave` / `spin_unlock_irqrestore` pair with `local_irq_save` / `local_irq_restore` (`include/asm/irqflags.h`).
- **`rq.lock`** — protects that CPU’s runnable list (`tasks`, `nr_running`, `pick_next_task`, `schedule()`). `migrate_task()` locks two rqs in CPU-id order.
- **`tasklist_lock`** — protects the global `all_tasks` list (`task_attach`, `task_detach`, `for_each_task` walkers). Kept separate from `rq.lock` so runqueue and task-enumeration locking do not alias.
- **`uart_lock` / `tty0.lock`** — serialize PL011 MMIO and the console TTY RX ring / termios (see Console below).
- `include/linux/wait.h` + `kernel/sched/wait.c` — Linux-style wait queues for blocking I/O:
  - `DECLARE_WAITQUEUE` on the stack, `prepare_to_wait` → `schedule` → `finish_wait`.
  - `wait_event(wq, condition)` — sleep until a function-pointer condition is true.
  - `wait_event_interruptible(wq, condition)` — same, but returns `-ERESTARTSYS` if a signal is pending (`SA_RESTART` may rewind the syscall).
  - `wake_up()` marks sleeping waiters runnable via `wake_up_process()`.
  - Used by `fs/pipe.c` (read/write when the buffer is empty/full) and `drivers/tty/tty.c` (`wait_event_interruptible` on blocking read).
- `include/linux/list.h` — doubly-linked `list_head` helpers (`list_add`, `list_del_init`, `list_is_linked`, `list_for_each`). Off-list nodes are self-linked (`INIT_LIST_HEAD` / `list_del_init`); never use bare `list_del`.
- `include/linux/rbtree.h` + `lib/rbtree.c` — minimal red-black tree (insert, erase, in-order walk).

There are no mutexes, RW locks, or `rcu` — spinlocks plus IRQ masking cover the current SMP-safe paths.

### System calls

`kernel/sys.c` dispatches on `regs->x8` (Linux AArch64 ABI: number in `x8`, args in `x0`–`x5`, return in `x0`). After the call, `do_signal()` runs before returning to EL0.

Implemented (subset):

- I/O and files: `read`, `write`, `writev`, `openat` (`O_CLOEXEC`), `close`, `dup`/`dup3` (libc `dup2()` uses `dup3` on AArch64; `dup2(fd,fd)` is a no-op via `fcntl`), `pipe2` (`O_CLOEXEC`, `O_NONBLOCK`), `ppoll` (BusyBox `poll()`; TTY/pipe wait queues), `fstat`, `newfstatat`, `getdents64`, `lseek`, `fcntl` (`F_DUPFD`, `F_DUPFD_CLOEXEC`, `F_GET/SETFD`, `F_GET/SETFL`), `sendfile`
- Paths: `mkdirat`, `unlinkat`, `linkat`, `symlinkat`, `readlinkat`, `renameat`, `faccessat` / `faccessat2`, `chdir`, `fchdir`, `getcwd`, `fchmod` / `fchmodat`, `utimensat` (stub). Relative `*at` names start at `dfd` (`AT_FDCWD` = cwd; absolute names ignore `dfd`). Permission bits live in `inode->i_mode`.
- Processes: `clone` (always fork), `execve`, `exit` / `exit_group`, `wait4`, `getpid` / `gettid` / `getppid`, `getpgrp`, `setpgid`, `getsid`, `setsid`, `sched_yield`, `set_tid_address`
- Identity / time: `getuid` / `geteuid` / `getgid` / `getegid` (all 0), `uname`, `clock_gettime` / `clock_settime` (`CLOCK_REALTIME` from PL031, else `jiffies`), `nanosleep`, `sysinfo`
- Memory: `brk`, `mmap` (anonymous), `munmap`, `mprotect` (VMA split + COW-safe PTEs)
- Signals: `kill`, `rt_sigaction`, `rt_sigprocmask`, `rt_sigpending`, `rt_sigsuspend`, `rt_sigreturn`
- Reboot: `reboot` (`RESTART`, `HALT`, `POWER_OFF` via PSCI on QEMU virt)
- TTY `ioctl`: `TCGETS`/`TCSETS`, `TIOCGPGRP`/`TIOCSPGRP`, `TIOCSCTTY`, `TIOCGSID`, `TIOCNOTTY`, `TIOCGWINSZ`

Unknown numbers return `-ENOSYS`.

### Signals

`kernel/signal.c` is a small Linux rt-signal path:

- Each task has `pending` and `blocked` bitmasks (Linux layout: signal `n` is bit `n-1`).
- `kill` queues a bit (`pid > 0` one task, `pid == 0` caller’s pgrp, `pid < -1` another pgrp, `pid == -1` all user tasks except caller) and wakes a sleeper if the signal is unblocked.
- **`wait4`**, pipe I/O, and TTY read are interruptible: they return kernel-internal **`-ERESTARTSYS`**. If the delivered handler has **`SA_RESTART`**, `do_signal` restores orig `x0` and rewinds `ELR` by 4 so `svc` runs again; otherwise userspace sees **`-EINTR`**. Ignored signals (e.g. default `SIGCHLD`) also restart. `sigsuspend` always returns `-EINTR`.
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

- `mm/page_alloc.c` — buddy allocator (`alloc_pages` / `free_pages`); `mem_map[]` of `struct page` with `atomic_t _refcount`; `virt_to_page` / `page_address` / `get_page` / `put_page` / `page_count` (fork COW).
- `include/linux/atomic.h` — minimal `atomic_t` helpers.
- `include/linux/mm_types.h` — `struct page`, VMA, `mm_struct`.
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
  - Helpers: `find_vma`, `vma_record`, `vma_erase_range` / `vma_split` (trim / split / remove; split failure returns `-ENOMEM`), `dup_vma_list`, `free_all_vmas`.
  - `do_map` / page-table walk — map anonymous pages into TTBR0 tables.
  - `do_brk` / `do_mmap` / `do_munmap` — grow or shrink the heap VMA, allocate anonymous regions (`MAP_ANONYMOUS`, optional `MAP_FIXED`), and unmap pages while keeping the VMA list consistent.
  - **`mprotect`** — `ksys_mprotect` validates alignment, `prot` bits, and overflow; `mm_mprotect()` requires the range to be fully covered (holes → `-ENOMEM`), splits VMAs at the edges, updates `vm_flags`, then updates present PTEs. Unmapped leaves are left for demand paging. Shared COW pages stay hardware read-only even if the VMA is writable (`page_count > 1`).
  - Fork COW: `dup_pgtable` shares leaf pages, wrprotects both sides, `get_page`; write faults call `do_cow_fault`.
  - `mm_alloc` / `dup_mm` / `mm_put` — allocate or duplicate `mm_struct` (VMA list + COW page tables); last user `put_page`s tables and VMAs, then `kfree(mm)`.
- `mm/uaccess.c` — `copy_to_user` / `copy_from_user` (EL1 access to EL0 mappings).
- `fs/exec.c` / `fs/binfmt.c` — `execve`: load an **AArch64 `ET_EXEC`** ELF, map a user stack, `vma_record` each `PT_LOAD` and the stack, copy argv/envp from the caller (NULL-terminated, up to 32 strings), build Linux-style argc/argv/envp/auxv, reset non-`SIG_IGN` handlers to `SIG_DFL`, then drop the old `mm`.

User addresses sit in a low range (stack near `0x4040000`, mmap base `0x2000000`). Kernel virtual memory is the high half. Each user task has its own `pgd`; `mm_install()` switches it on context switch. There is no shared-mm `vfork`.

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
- `fs/inode.c` — `iget` / `iput`; `i_count` plus `nlink`. Unlinked open files stay until last `fput`. Proc pid inodes are non-persistent (`iput` at 0 frees them).
- `fs/namei.c` — path resolve, `mkdir`/`mknod`/`unlink`/`link`/`symlink`/`rename`/`chdir`/`fchdir`; `getname_from_user_at` (relative `*at` from `dfd`); final-component symlink follow (depth 8).
- `fs/ramfs.c` — in-memory files and directories, hard links, symlinks (`S_IFLNK`), char nodes (`S_IFCHR`); nodes and data buffers via `kmalloc`.
- `fs/pipe.c` — anonymous pipes (`kmalloc` for `struct pipe`); `pipe2` accepts `O_CLOEXEC` / `O_NONBLOCK`. Blocking I/O waits on the ring (`-ERESTARTSYS` if a signal is pending); `O_NONBLOCK` returns `-EAGAIN` instead of sleeping. `poll` reports `POLLIN` / `POLLOUT` / `POLLHUP`.
- `fs/poll.c` — `ppoll(2)` (AArch64 has no `poll`); ash arrow keys use this on stdin.
- `fs/open.c`, `fs/read_write.c`, `fs/stat.c`, `fs/readdir.c` — fd table (`alloc_file()` → `kmalloc`), `fcntl`, `lseek` via `f_op->llseek`.
- `fs/procfs.c` — `/proc`, `/proc/<pid>/stat`, `/proc/<pid>/cmdline` for BusyBox `ps`; `/proc/schedstat` (`cpuN switches pulls ipis`).
- `fs/char_dev.c` — `register_chrdev` / `chrdev_open` by major (mem=1, tty=5).
- `fs/dev.c`, `fs/devnull.c`, `fs/devtty.c`, `fs/devconsole.c` — create `/dev` nodes with `mknod` and open via the chrdev table.
- `fs/initramfs.c` — unpack a newc cpio blob into ramfs.

There is no block layer, no ext4, no mount table beyond “everything is ramfs (+ proc + char devices).” Shebang (`#!`) execution is not supported — run scripts as `/bin/sh script`.

### Console, printk, and SMP

- `drivers/tty/serial.c` — PL011 UART; **`uart_lock`** serializes all MMIO so concurrent `printk` / `tty_write` / RX on different CPUs cannot corrupt TX or wedge the FIFO. `uart_write` / `serial_write_n` hold the lock for a whole string so lines stay intact. RX IRQ drops the lock before `tty_receive_char()` so echo cannot deadlock.
- `kernel/printk.c` — `printk()` and Linux-style `pr_info` / `pr_err` / … macros (`include/linux/printk.h`); output goes to UART with `KERN_*` level prefixes.
- `lib/vsnprintf.c` — minimal formatter (`%d`, `%u`, `%x`, `%lx`, `%p`, `%s`, `%c`, `%%`) used by `printk`.
- `drivers/tty/tty.c` — canonical line discipline, echo, job-control signals, termios (`TCGETS`/`TCSETS`), controlling TTY (`TIOCSCTTY`), blocking read via `wait_event_interruptible`, winsize stub. **`tty0.lock`** protects the RX ring, termios flags, and session/pgrp against readers and `serial_irq` on another CPU; echo calls `serial_putc` outside that lock.
- `kernel/smp.c` — bring up secondary CPUs with `psci_cpu_on`; each gets its own idle task, runqueue, timer, and `TPIDR_EL1`. `send_reschedule_ipi` / `handle_reschedule_ipi` set `need_resched` on the target CPU after migration or wake.

PID 1 gets fd 0/1/2 on the UART TTY before `kernel_execve("/init")`.

### Userspace

**BusyBox** is the main userspace: static binary at `initramfs/busybox`, copied to `/bin/busybox` with symlinks for common applets (`ash`, `sh`, `ls`, `echo`, `cat`, `mv`, `sleep`, `ps`, `uname`, `true`, `false`, `pwd`, `date`, `halt`, `poweroff`, `reboot` — see `BUSYBOX_APPLETS` in the Makefile).

Boot chain:

1. Kernel → `/init` (busybox symlink, `init` applet).
2. `/etc/inittab` → `sysinit` runs `/bin/sh /etc/init.d/rcS`, then `respawn` starts login ash.
3. `/etc/profile` sets `PATH=/bin:/sbin`, `HOME=/`, `TERM=linux`, `TZ` (POSIX `CST-8` = UTC+8). PID 1 also gets `TZ` from `kernel_execve` so `date` works even if ash does not source the profile.

**`/bin/hello`** is a separate musl program (`initramfs/src/hello.c`) used to regression-test syscalls. It is not part of normal boot.

A glibc or dynamically linked userspace will not run. Programs must be static AArch64 `ET_EXEC` ELFs.

### Userspace kselftests

Linux-shaped tree under `tools/testing/selftests/` (header + `lib.mk` + per-suite dirs + `run_kselftest.sh`). Built with the musl toolchain and installed into the initramfs as `/kselftests/`. In-kernel KUnit-style tests are not present yet.

```sh
make kselftest          # build + install into initramfs/root/kselftests
# after boot (no shebang exec yet — invoke via ash):
sh /kselftests/run_kselftest.sh
/kselftests/yield/yield_test
/kselftests/mm/cow_test
/kselftests/mm/mprotect_test
/kselftests/files/dup_test
/kselftests/files/pipe_test
/kselftests/files/rename_test
/kselftests/files/open_test
/kselftests/files/access_test
/kselftests/files/at_test
/kselftests/files/inode_test
/kselftests/files/chmod_test
/kselftests/files/exec_env_test
/kselftests/signals/sa_restart_test
/kselftests/signals/sigset_test
/kselftests/time/realtime_test
/kselftests/sched/fork_busy_test
/kselftests/sched/sleep_spin_test
/kselftests/sched/yield_pong_test
/kselftests/sched/hog_smp_test
/kselftests/sched/sched_bench_test
```

- `kselftest.h` — TAP helpers (`ksft_print_header`, `ksft_set_plan`, `ksft_test_result`, `ksft_finished`), same usage pattern as Linux.
- `yield/yield_test.c` — sample suite: `sched_yield` returns 0.
- `mm/cow_test.c` — basic anon COW after `fork` (child write; inspired by Linux `selftests/mm/cow.c`).
- `mm/mprotect_test.c` — `mprotect` args, holes, VMA split, COW-safe RW.
- `files/dup_test.c` — `dup2` / `dup3` (same fd, replace, `O_CLOEXEC`).
- `files/pipe_test.c` — `pipe2` flags, non-blocking empty/full I/O, `F_SETFL`.
- `files/poll_test.c` — `poll`/`ppoll` on files and pipes (`POLLIN`, `POLLOUT`, `POLLNVAL`, `POLLHUP`).
- `files/rename_test.c` — `renameat` (file, into dir, replace, type mismatch, descendant).
- `files/open_test.c` — `openat` `O_CLOEXEC` (file, chrdev, closed across `exec`).
- `files/access_test.c` — `faccessat` / `faccessat2` (`F_OK`, `R_OK`/`W_OK`/`X_OK`, `AT_EMPTY_PATH`).
- `files/at_test.c` — relative `openat` / `faccessat` / `mkdirat` from a directory fd; `fchdir`.
- `files/inode_test.c` — unlink of an open file; proc `iget`/`iput`.
- `files/chmod_test.c` — `fchmod` / `fchmodat` / `chmod`; `access` after `chmod 000`.
- `files/exec_env_test.c` — `execve` copies `envp` onto the new stack.
- `signals/sa_restart_test.c` — `SA_RESTART` rewinds a blocking `read`; without it, `EINTR`.
- `signals/sigset_test.c` — `sa_mask`, `sigprocmask`, `sigpending` (Linux bit layout).
- `time/realtime_test.c` — `clock_gettime` / `clock_settime` (`CLOCK_REALTIME`).
- `sched/fork_busy_test.c` — 8 busy `fork` children all exit and get `waitpid` (none stuck `RUNNING`).
- `sched/sleep_spin_test.c` — `nanosleep` child wakes while a spinner stays `RUNNING`.
- `sched/yield_pong_test.c` — two tasks ping-pong a shared counter with `sched_yield`.
- `sched/hog_smp_test.c` — 8 CPU hogs on `-smp 4`; fork placement should show them on all 4 CPUs (`/proc/<pid>/stat` field 39).
- `sched/sched_bench_test.c` — 8 yield hogs; prints `switches/s` from `/proc/schedstat` (TCG is noisy; compare ratios, not ns).

To add a suite: create `tools/testing/selftests/foo/`, add a `Makefile` with `TEST_GEN_PROGS` and `include ../lib.mk`, then append `foo` to `TARGETS` in `tools/testing/selftests/Makefile`.

## How a syscall looks

1. User `svc #0`.
2. `sync_el0_entry` saves GPRs, ELR, SPSR into `pt_regs` on the kernel stack.
3. `handle_syscall()` runs with IRQs masked around return (so ELR/SPSR are not clobbered).
4. `do_signal()` may change ELR to a handler and push a user stack frame.
5. `finish_eret` restores registers and returns to EL0.

Fork copies that frame onto the child’s kernel stack and points the child at `ret_from_fork`.

## What is deliberately missing

No `siginfo`, no `ptrace`, no networking, no disk. No mutexes or reader/writer locks yet. No CFS / push balancing (pull only). No PIE loader, no `ld.so`. No file-backed `mmap`. No full driver model (sysfs / `cdev` kobjects); char devices are a major→open table plus ramfs `mknod`. No shebang interpreter. Many syscalls BusyBox can optionally use are still absent: `vhangup`, mount/unmount, etc.

Names like `task_struct` are there so you can grep Linux later and recognize the shape—not so this can merge with Linux.

## Reading order

1. `kernel/head.S` → `init/main.c`
2. `kernel/entry.S` → `kernel/sys.c`
3. `kernel/smp.c` → `include/asm/smp.h` → `kernel/irq.c` → `kernel/time/tick.c`
4. `kernel/sched/core.c` → `kernel/sched/wait.c` → `include/linux/spinlock.h` → `mm/slub.c` → `kernel/fork.c` → `kernel/exit.c`
5. `include/linux/list.h` → `include/linux/rbtree.h` → `lib/rbtree.c`
6. `include/linux/mm_types.h` → `mm/mmap.c` (VMAs, `do_brk` / `do_mmap` / `do_munmap` / `mm_mprotect`, COW)
7. `fs/ramfs.c` → `fs/dcache.c` → `fs/namei.c`
8. `fs/binfmt.c` → `fs/exec.c`
9. `fs/procfs.c` → `fs/dev.c`
10. `drivers/tty/serial.c` → `drivers/tty/tty.c` → `kernel/printk.c`
11. `kernel/signal.c` → `kernel/reboot.c` → `kernel/psci.c`
12. `tools/testing/selftests/` (userspace TAP) and `initramfs/src/hello.c`
13. `initramfs/etc/inittab`, `initramfs/etc/profile`
