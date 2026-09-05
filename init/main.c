/*
 * miniSMP kernel — AArch64 bare-metal on QEMU virt
 */

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/tick.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <asm/smp.h>

#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ramfs.h>
#include <linux/dcache.h>
#include <linux/initramfs.h>
#include <linux/binfmts.h>
#include <linux/proc_fs.h>
#include <linux/devnull.h>
#include "test.h"

extern char __initramfs_start[];
extern char __initramfs_end[];

static void rest_init(void)
{
    struct task_struct *init;

    init = kernel_thread(kernel_init, 0);
    if (!init) {
        pr_err("kernel_thread failed\n");
        return;
    }

    wake_up_process(init);

    pr_info("Rest init: PID 1 created, boot thread -> idle\n");
}

void start_kernel(void)
{
    serial_init();
    time_init();
    tty_init();

    smp_init();
    bringup_nonboot_cpus();

    pr_info("All secondary CPUs are online\n");

    page_alloc_init();
    slub_init();
    mmap_init();

    ramfs_init();
    dcache_init();

    if ((unsigned long)__initramfs_end > (unsigned long)__initramfs_start) {
        unsigned long size = (unsigned long)(__initramfs_end -
                                               __initramfs_start);
        int err = unpack_to_rootfs(__initramfs_start, size);

        if (err)
            pr_err("unpack_to_rootfs failed\n");
        else
            pr_info("unpack_to_rootfs: ok\n");
    }

    proc_init();
    devnull_init();

    sched_init();
    run_kernel_tests();
    rest_init();

    cpu_idle();
}

static void ramfs_list_entry(const char *name, struct inode *inode, void *arg)
{
    (void)arg;

    pr_info("  %s%s\n", name, inode_is_dir(inode) ? "/" : "");
}

void initramfs_show(void)
{
    pr_info("rootfs listing:\n");
    ramfs_readdir(ramfs_root(), ramfs_list_entry, 0);
}

static void init_stdio(struct task_struct *task)
{
    get_file(&uart_file);
    task->files[0] = &uart_file;
    get_file(&uart_file);
    task->files[1] = &uart_file;
    get_file(&uart_file);
    task->files[2] = &uart_file;

    tty_attach_session(task->sid, task->pgid);
}

void kernel_init(void *arg)
{
    int ret;

    (void)arg;

    pr_info("Init (PID 1) running\n");

    init_stdio(current);

    initramfs_show();

    ret = kernel_execve("/init");
    if (ret == 0)
        return;

    pr_err("Failed to execute /init\n");

    for (;;)
        __asm__ volatile("wfi");
}
