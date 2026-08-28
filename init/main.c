/*
 * miniSMP kernel — AArch64 bare-metal on QEMU virt
 */

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/tick.h>
#include <linux/fs.h>
#include <asm/smp.h>

#include <linux/gfp.h>
#include <linux/ramfs.h>
#include <linux/dcache.h>
#include <linux/initramfs.h>
#include <linux/binfmts.h>
#include <linux/proc_fs.h>

extern char __initramfs_start[];
extern char __initramfs_end[];

static void rest_init(void)
{
    struct task_struct *init;

    init = kernel_thread(kernel_init, 0);
    if (!init) {
        uart_puts("kernel_thread failed\n");
        return;
    }

    wake_up_process(init);
    runqueue = init;

    uart_puts("Rest init: PID 1 created, boot thread -> idle\n");
}

static unsigned int cpu_id(void)
{
    unsigned long mpidr;

    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (unsigned int)(mpidr & 0xff);
}

void start_kernel(void)
{
    unsigned int id = cpu_id();

    if (id == 0) {
        serial_init();
        time_init();
        tty_init();

        uart_puts("Hello from CPU0\n");
        smp_init();
        bringup_nonboot_cpus();

        for (unsigned int i = 1; i < NR_CPUS; i++) {
            uart_puts("Hello from CPU");
            uart_putc('0' + (char)i);
            uart_puts("\n");
        }

        page_alloc_init();

        ramfs_init();
        dcache_init();

        if ((unsigned long)__initramfs_end > (unsigned long)__initramfs_start) {
            unsigned long size = (unsigned long)(__initramfs_end -
                                                   __initramfs_start);
            int err = unpack_to_rootfs(__initramfs_start, size);

            if (err)
                uart_puts("unpack_to_rootfs failed\n");
            else
                uart_puts("unpack_to_rootfs: ok\n");
        }

        proc_init();

        sched_init();
        rest_init();

        for (unsigned int i = 0; i < NR_CPUS; i++) {
            uart_puts("CPU");
            uart_putc('0' + (char)i);
            uart_puts(" idle task (PID 0) ready\n");
        }
    }

    cpu_idle();
}

static void ramfs_list_entry(const char *name, struct inode *inode, void *arg)
{
    (void)arg;

    uart_puts("  ");
    uart_puts(name);
    uart_puts(inode_is_dir(inode) ? "/\n" : "\n");
}

void initramfs_show(void)
{
    uart_puts("rootfs listing:\n");
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

    uart_puts("Init (PID 1) running\n");

    init_stdio(current);

    initramfs_show();

    ret = kernel_execve("/init");
    if (ret == 0)
        return;

    uart_puts("Failed to execute /init\n");

    for (;;)
        __asm__ volatile("wfi");
}
