/*
 * reboot(2) — restart or power off via PSCI (QEMU virt / ARM64 firmware).
 */

#include <linux/reboot.h>
#include <linux/sched/task.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <asm/irqflags.h>
#include <asm/psci.h>
#include <asm/signal.h>

static void kernel_restart(void)
{
    pr_info("Restarting system...\n");
    local_irq_disable();

    for (;;)
        (void)psci_hvc(PSCI_0_2_FN_SYSTEM_RESET, 0, 0, 0);
}

static void kernel_poweroff(void)
{
    pr_info("Powering off...\n");
    local_irq_disable();

    psci_hvc(PSCI_0_2_FN_SYSTEM_OFF, 0, 0, 0);

    for (;;)
        __asm__ volatile("wfi");
}

void kernel_init_shutdown(int sig)
{
    /*
     * BusyBox (when not PID 1): kill_all(), then kill(1, sig) and exit.
     * Init must shut down. This BusyBox build uses SIGUSR1/SIGUSR2 for
     * halt/poweroff; map both to power off. Use "reboot -f" to restart
     * (calls reboot(2) directly with RB_AUTOBOOT).
     */
    if (sig == SIGUSR1 || sig == SIGUSR2 || sig == SIGTERM)
        kernel_poweroff();
}

static int reboot_magic2_ok(unsigned int magic2)
{
    return magic2 == LINUX_REBOOT_MAGIC2 ||
           magic2 == LINUX_REBOOT_MAGIC2A ||
           magic2 == LINUX_REBOOT_MAGIC2B ||
           magic2 == LINUX_REBOOT_MAGIC2C;
}

long ksys_reboot(unsigned int magic1, unsigned int magic2, unsigned int cmd,
                 void *arg)
{
    (void)arg;

    if (!current || !current->is_user)
        return -EINVAL;

    if (magic1 != LINUX_REBOOT_MAGIC1 || !reboot_magic2_ok(magic2))
        return -EINVAL;

    switch (cmd) {
    case LINUX_REBOOT_CMD_RESTART:
        kernel_restart();
        break;

    case LINUX_REBOOT_CMD_HALT:
    case LINUX_REBOOT_CMD_POWER_OFF:
        kernel_poweroff();
        break;

    default:
        return -EINVAL;
    }

    return 0; /* not reached */
}
