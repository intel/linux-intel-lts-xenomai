/*   -*- linux-c -*-
 *   linux/arch/x86/kernel/ipipe.c
 *
 *   Copyright (C) 2002-2012 Philippe Gerum.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *   Architecture-dependent I-PIPE support for x86.
 */

#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/extable.h>
#include <linux/ipipe_tickdev.h>
#include <asm/asm-offsets.h>
#include <asm/unistd.h>
#include <asm/processor.h>
#include <asm/atomic.h>
#include <asm/hw_irq.h>
#include <asm/irq.h>
#include <asm/desc.h>
#include <asm/io.h>
#ifdef CONFIG_X86_LOCAL_APIC
#include <asm/tlbflush.h>
#include <asm/fixmap.h>
#include <asm/bitops.h>
#include <asm/mpspec.h>
#ifdef CONFIG_X86_IO_APIC
#include <asm/io_apic.h>
#endif	/* CONFIG_X86_IO_APIC */
#include <asm/apic.h>
#endif	/* CONFIG_X86_LOCAL_APIC */
#include <asm/fpu/internal.h>
#include <asm/traps.h>
#include <asm/tsc.h>
#include <asm/mce.h>
#include <asm/mmu_context.h>

void smp_apic_timer_interrupt(struct pt_regs *regs);
void smp_kvm_posted_intr_wakeup_ipi(struct pt_regs *regs);
void smp_kvm_posted_intr_ipi(struct pt_regs *regs);
void smp_spurious_interrupt(struct pt_regs *regs);
void smp_error_interrupt(struct pt_regs *regs);
void smp_x86_platform_ipi(struct pt_regs *regs);
void smp_irq_work_interrupt(struct pt_regs *regs);
void smp_reschedule_interrupt(struct pt_regs *regs);
void smp_call_function_interrupt(struct pt_regs *regs);
void smp_call_function_single_interrupt(struct pt_regs *regs);
void smp_irq_move_cleanup_interrupt(struct pt_regs *regs);
void smp_reboot_interrupt(void);
void smp_thermal_interrupt(struct pt_regs *regs);
void smp_threshold_interrupt(struct pt_regs *regs);

DEFINE_PER_CPU(unsigned long, __ipipe_cr2);
EXPORT_PER_CPU_SYMBOL_GPL(__ipipe_cr2);

int ipipe_get_sysinfo(struct ipipe_sysinfo *info)
{
	info->sys_nr_cpus = num_online_cpus();
	info->sys_cpu_freq = __ipipe_cpu_freq;
	info->sys_hrtimer_irq = per_cpu(ipipe_percpu.hrtimer_irq, 0);
	info->sys_hrtimer_freq = __ipipe_hrtimer_freq;
	info->sys_hrclock_freq = __ipipe_hrclock_freq;

	return 0;
}
EXPORT_SYMBOL_GPL(ipipe_get_sysinfo);

static void __ipipe_do_IRQ(unsigned int irq, void *cookie)
{
	void (*handler)(struct pt_regs *regs);
	struct pt_regs *regs;

	regs = raw_cpu_ptr(&ipipe_percpu.tick_regs);
	regs->orig_ax = ~__ipipe_get_irq_vector(irq);
	handler = (typeof(handler))cookie;
	handler(regs);
}

#ifdef CONFIG_X86_LOCAL_APIC

static void __ipipe_noack_apic(struct irq_desc *desc)
{
}

static void __ipipe_ack_apic(struct irq_desc *desc)
{
	__ack_APIC_irq();
}

#endif	/* CONFIG_X86_LOCAL_APIC */

/*
 * __ipipe_enable_pipeline() -- We are running on the boot CPU, hw
 * interrupts are off, and secondary CPUs are still lost in space.
 */
void __init __ipipe_enable_pipeline(void)
{
	unsigned int irq;

#ifdef CONFIG_X86_LOCAL_APIC

	/* Map the APIC system vectors. */

	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(LOCAL_TIMER_VECTOR),
			  __ipipe_do_IRQ, smp_apic_timer_interrupt,
			  __ipipe_ack_apic);

#ifdef CONFIG_HAVE_KVM
	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(POSTED_INTR_WAKEUP_VECTOR),
			  __ipipe_do_IRQ, smp_kvm_posted_intr_wakeup_ipi,
			  __ipipe_ack_apic);

	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(POSTED_INTR_VECTOR),
			  __ipipe_do_IRQ, smp_kvm_posted_intr_ipi,
			  __ipipe_ack_apic);
#endif

#if defined(CONFIG_X86_MCE_AMD) && defined(CONFIG_X86_64)
	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(DEFERRED_ERROR_VECTOR),
			  __ipipe_do_IRQ, smp_deferred_error_interrupt,
			  __ipipe_ack_apic);
#endif

#ifdef CONFIG_X86_UV
	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(UV_BAU_MESSAGE),
			  __ipipe_do_IRQ, uv_bau_message_interrupt,
			  __ipipe_ack_apic);
#endif /* CONFIG_X86_UV */

	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(SPURIOUS_APIC_VECTOR),
			  __ipipe_do_IRQ, smp_spurious_interrupt,
			  __ipipe_noack_apic);

	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(ERROR_APIC_VECTOR),
			  __ipipe_do_IRQ, smp_error_interrupt,
			  __ipipe_ack_apic);

#ifdef CONFIG_X86_THERMAL_VECTOR
	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(THERMAL_APIC_VECTOR),
			  __ipipe_do_IRQ, smp_thermal_interrupt,
			  __ipipe_ack_apic);
#endif /* CONFIG_X86_THERMAL_VECTOR */

#ifdef CONFIG_X86_MCE_THRESHOLD
	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(THRESHOLD_APIC_VECTOR),
			  __ipipe_do_IRQ, smp_threshold_interrupt,
			  __ipipe_ack_apic);
#endif /* CONFIG_X86_MCE_THRESHOLD */

	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(X86_PLATFORM_IPI_VECTOR),
			  __ipipe_do_IRQ, smp_x86_platform_ipi,
			  __ipipe_ack_apic);

	/*
	 * We expose two high priority APIC vectors the head domain
	 * may use respectively for hires timing and SMP rescheduling.
	 * We should never receive them in the root domain.
	 */
	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(IPIPE_HRTIMER_VECTOR),
			  __ipipe_do_IRQ, smp_spurious_interrupt,
			  __ipipe_ack_apic);

	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(IPIPE_RESCHEDULE_VECTOR),
			  __ipipe_do_IRQ, smp_spurious_interrupt,
			  __ipipe_ack_apic);

#ifdef CONFIG_IRQ_WORK
	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(IRQ_WORK_VECTOR),
			  __ipipe_do_IRQ, smp_irq_work_interrupt,
			  __ipipe_ack_apic);
#endif /* CONFIG_IRQ_WORK */

#endif	/* CONFIG_X86_LOCAL_APIC */

#ifdef CONFIG_SMP
	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(RESCHEDULE_VECTOR),
			  __ipipe_do_IRQ, smp_reschedule_interrupt,
			  __ipipe_ack_apic);

	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(CALL_FUNCTION_VECTOR),
			  __ipipe_do_IRQ, smp_call_function_interrupt,
			  __ipipe_ack_apic);

	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(CALL_FUNCTION_SINGLE_VECTOR),
			  __ipipe_do_IRQ, smp_call_function_single_interrupt,
			  __ipipe_ack_apic);

	ipipe_request_irq(ipipe_root_domain,
			  IRQ_MOVE_CLEANUP_VECTOR,
			  __ipipe_do_IRQ, smp_irq_move_cleanup_interrupt,
			  __ipipe_ack_apic);

	ipipe_request_irq(ipipe_root_domain,
			  ipipe_apic_vector_irq(REBOOT_VECTOR),
			  __ipipe_do_IRQ, smp_reboot_interrupt,
			  __ipipe_ack_apic);
#endif	/* CONFIG_SMP */

	/*
	 * Finally, request the remaining ISA and IO-APIC
	 * interrupts. Interrupts which have already been requested
	 * will just beget a silent -EBUSY error, that's ok.
	 */
	for (irq = 0; irq < IPIPE_NR_XIRQS; irq++)
		ipipe_request_irq(ipipe_root_domain, irq,
				  __ipipe_do_IRQ, do_IRQ,
				  NULL);
}

#ifdef CONFIG_SMP
int irq_activate(struct irq_desc *desc);

int ipipe_set_irq_affinity(unsigned int irq, cpumask_t cpumask)
{
	struct irq_desc *desc;
	struct irq_chip *chip;
	int err;

	cpumask_and(&cpumask, &cpumask, cpu_online_mask);
	if (cpumask_empty(&cpumask) || ipipe_virtual_irq_p(irq))
		return -EINVAL;

	desc = irq_to_desc(irq);
	if (desc == NULL)
		return -EINVAL;

	chip = irq_desc_get_chip(desc);
	if (chip->irq_set_affinity == NULL)
		return -ENOSYS;

	err = irq_activate(desc);
	if (err)
		return err;

	chip->irq_set_affinity(irq_get_irq_data(irq), &cpumask, true);

	return 0;
}
EXPORT_SYMBOL_GPL(ipipe_set_irq_affinity);

void ipipe_send_ipi(unsigned int ipi, cpumask_t cpumask)
{
	unsigned long flags;

	flags = hard_local_irq_save();

	cpumask_clear_cpu(ipipe_processor_id(), &cpumask);
	if (likely(!cpumask_empty(&cpumask)))
		apic->send_IPI_mask(&cpumask, ipipe_apic_irq_vector(ipi));

	hard_local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(ipipe_send_ipi);

void __ipipe_hook_critical_ipi(struct ipipe_domain *ipd)
{
	unsigned int ipi = IPIPE_CRITICAL_IPI;

	ipd->irqs[ipi].ackfn = __ipipe_ack_apic;
	ipd->irqs[ipi].handler = __ipipe_do_critical_sync;
	ipd->irqs[ipi].cookie = NULL;
	ipd->irqs[ipi].control = IPIPE_HANDLE_MASK|IPIPE_STICKY_MASK;
}

#endif	/* CONFIG_SMP */

void __ipipe_halt_root(int use_mwait)
{
	struct ipipe_percpu_domain_data *p;

	/* Emulate sti+hlt sequence over the root domain. */

	hard_local_irq_disable();

	p = ipipe_this_cpu_root_context();

	trace_hardirqs_on();
	__clear_bit(IPIPE_STALL_FLAG, &p->status);

	if (unlikely(__ipipe_ipending_p(p))) {
		__ipipe_sync_stage();
		hard_local_irq_enable();
	} else {
#ifdef CONFIG_IPIPE_TRACE_IRQSOFF
		ipipe_trace_end(0x8000000E);
#endif /* CONFIG_IPIPE_TRACE_IRQSOFF */
		if (use_mwait)
			asm volatile("sti; .byte 0x0f, 0x01, 0xc9;"
				     :: "a" (0), "c" (0));
		else
			asm volatile("sti; hlt": : :"memory");
	}
}
EXPORT_SYMBOL_GPL(__ipipe_halt_root);

static inline void __ipipe_fixup_if(bool stalled, struct pt_regs *regs)
{
	/*
	 * Have the saved hw state look like the domain stall bit, so
	 * that __ipipe_unstall_iret_root() restores the proper
	 * pipeline state for the root stage upon exit.
	 */
	if (stalled)
		regs->flags &= ~X86_EFLAGS_IF;
	else
		regs->flags |= X86_EFLAGS_IF;
}

dotraplinkage int __ipipe_trap_prologue(struct pt_regs *regs, int trapnr, unsigned long *flags)
{
	bool entry_irqs_off = hard_irqs_disabled();
	struct ipipe_domain *ipd;
	unsigned long cr2;

	if (trapnr == X86_TRAP_PF)
		cr2 = native_read_cr2();

	/*
	 * KGDB and ftrace may poke int3/debug ops into the kernel
	 * code. Trap those exceptions early, do conditional fixups to
	 * the interrupt state depending on the current domain, let
	 * the regular handler see them.
	 */
	if (unlikely(!user_mode(regs) &&
		     (trapnr == X86_TRAP_DB || trapnr == X86_TRAP_BP))) {

		if (ipipe_root_p)
			goto root_fixup;

		/*
		 * Skip interrupt state fixup from the head domain,
		 * but do call the regular handler which is assumed to
		 * run fine within such context.
		 */
		return -1;
	}

	/*
	 * Now that we have filtered out all debug traps which might
	 * happen anywhere in kernel code in theory, detect attempts
	 * to probe kernel memory (i.e. calls to probe_kernel_{read,
	 * write}()). If that happened over the head domain, do the
	 * fixup immediately then return right after upon success. If
	 * that fails, the kernel is likely to crash but let's follow
	 * the standard recovery procedure in that case anyway.
	 */
	if (unlikely(!ipipe_root_p && faulthandler_disabled())) {
		if (fixup_exception(regs, trapnr))
			return 1;
	}

	if (unlikely(__ipipe_notify_trap(trapnr, regs)))
		return 1;

	if (likely(ipipe_root_p)) {
	root_fixup:
		/*
		 * If no head domain is installed, or in case we faulted in
		 * the iret path of x86-32, regs->flags does not match the root
		 * domain state. The fault handler may evaluate it. So fix this
		 * up with the current state.
		 */
		local_save_flags(*flags);
		__ipipe_fixup_if(raw_irqs_disabled_flags(*flags), regs);

		/*
		 * Sync Linux interrupt state with hardware state on
		 * entry.
		 */
		if (entry_irqs_off)
			local_irq_disable();
	} else {
		/* Plan for restoring the original flags at fault. */
		*flags = regs->flags;

		/*
		 * Detect unhandled faults over the head domain,
		 * switching to root so that it can handle the fault
		 * cleanly.
		 */
		hard_local_irq_disable();
		ipd = __ipipe_current_domain;
		__ipipe_set_current_domain(ipipe_root_domain);

		/* Sync Linux interrupt state with hardware state on entry. */
		if (entry_irqs_off)
			local_irq_disable();

		ipipe_trace_panic_freeze();

		/* Always warn about user land and unfixable faults. */
		if (user_mode(regs) ||
		    !search_exception_tables(instruction_pointer(regs))) {
			printk(KERN_ERR "BUG: Unhandled exception over domain"
			       " %s at 0x%lx - switching to ROOT\n",
			       ipd->name, instruction_pointer(regs));
			dump_stack();
			ipipe_trace_panic_dump();
		} else if (IS_ENABLED(CONFIG_IPIPE_DEBUG)) {
			/* Also report fixable ones when debugging is enabled. */
			printk(KERN_WARNING "WARNING: Fixable exception over "
			       "domain %s at 0x%lx - switching to ROOT\n",
			       ipd->name, instruction_pointer(regs));
			dump_stack();
			ipipe_trace_panic_dump();
		}
	}

	if (trapnr == X86_TRAP_PF)
		write_cr2(cr2);

	return 0;
}

dotraplinkage
void __ipipe_trap_epilogue(struct pt_regs *regs,
			   unsigned long flags, unsigned long regs_flags)
{
	ipipe_restore_root(raw_irqs_disabled_flags(flags));
	__ipipe_fixup_if(raw_irqs_disabled_flags(regs_flags), regs);
}

static inline int __ipipe_irq_from_vector(int vector, int *irq)
{
	struct irq_desc *desc;

	if (vector >= FIRST_SYSTEM_VECTOR) {
		*irq = ipipe_apic_vector_irq(vector);
		return 0;
	}

	desc = __this_cpu_read(vector_irq[vector]);
	if (likely(!IS_ERR_OR_NULL(desc))) {
		*irq = irq_desc_get_irq(desc);
		return 0;
	}

	if (vector == IRQ_MOVE_CLEANUP_VECTOR) {
		*irq = vector;
		return 0;
	}

#ifdef CONFIG_X86_LOCAL_APIC
	__ack_APIC_irq();
#endif
	pr_err("unexpected IRQ trap at vector %#x\n", vector);
	return -1;
}

int __ipipe_handle_irq(struct pt_regs *regs)
{
	struct ipipe_percpu_data *p = __ipipe_raw_cpu_ptr(&ipipe_percpu);
	int irq, vector = regs->orig_ax, flags = 0;
	struct pt_regs *tick_regs;

	if (likely(vector < 0)) {
		if (__ipipe_irq_from_vector(~vector, &irq) < 0)
			goto out;
	} else { /* Software-generated. */
		irq = vector;
		flags = IPIPE_IRQF_NOACK;
	}

	ipipe_trace_irqbegin(irq, regs);

	/*
	 * Given our deferred dispatching model for regular IRQs, we
	 * only record CPU regs for the last timer interrupt, so that
	 * the timer handler charges CPU times properly. It is assumed
	 * that no other interrupt handler cares for such information.
	 */
	if (irq == p->hrtimer_irq || p->hrtimer_irq == -1) {
		tick_regs = &p->tick_regs;
		tick_regs->flags = regs->flags;
		tick_regs->cs = regs->cs;
		tick_regs->ip = regs->ip;
		tick_regs->bp = regs->bp;
#ifdef CONFIG_X86_64
		tick_regs->ss = regs->ss;
		tick_regs->sp = regs->sp;
#endif
		if (!__ipipe_root_p)
			tick_regs->flags &= ~X86_EFLAGS_IF;
	}

	__ipipe_dispatch_irq(irq, flags);

	if (user_mode(regs) && ipipe_test_thread_flag(TIP_MAYDAY))
		__ipipe_call_mayday(regs);

	ipipe_trace_irqend(irq, regs);

out:
	if (!__ipipe_root_p ||
	    test_bit(IPIPE_STALL_FLAG, &__ipipe_root_status))
		return 0;

	return 1;
}

void __ipipe_arch_share_current(int flags)
{
	struct task_struct *p = current;

	/*
	 * Setup a clean extended FPU state for kernel threads.
	 */
	if (p->mm == NULL)
		memcpy(&p->thread.fpu.state,
		       &init_fpstate, fpu_kernel_xstate_size);
}

struct task_struct *__switch_to(struct task_struct *prev_p,
				struct task_struct *next_p);
EXPORT_SYMBOL_GPL(do_munmap);
EXPORT_SYMBOL_GPL(__switch_to);
EXPORT_SYMBOL_GPL(show_stack);

#if defined(CONFIG_SMP) || defined(CONFIG_DEBUG_SPINLOCK)
EXPORT_SYMBOL(tasklist_lock);
#endif /* CONFIG_SMP || CONFIG_DEBUG_SPINLOCK */

#if defined(CONFIG_CC_STACKPROTECTOR) && defined(CONFIG_X86_64)
EXPORT_PER_CPU_SYMBOL_GPL(irq_stack_union);
#endif
