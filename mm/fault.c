/*
 * EL0 memory abort handling — demand paging entry point.
 */

#include <linux/mm.h>
#include <asm/sysreg.h>
#include <asm/ptrace.h>
#include <linux/sched/task.h>
#include <linux/signal.h>

void do_data_abort(struct pt_regs *regs)
{
    unsigned long esr;
    unsigned long addr;
    int ret;

    (void)regs;

    esr = read_esr_el1();
    addr = read_far_el1();

    ret = do_page_fault(current->mm, addr, esr);

    if (ret == 0)
        return;

    /*
     * Invalid memory access.
     *
     * Later this should become:
     *     send SIGSEGV
     *
     * For now:
     */
    do_exit(SIGSEGV);
}
