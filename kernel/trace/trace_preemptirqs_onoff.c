/*
 * trace irqs off preempt off preemptirqs off
 *
 *  Copyright (C) 2017
 */
#include <linux/ftrace.h>

#include "trace.h"

#include "preemptirqs_onoff_helper.h"

#define CREATE_TRACE_POINTS
#include <trace/events/preemptirqs_onoff.h>

#if defined(CONFIG_IRQS_ONOFF_TRACER)
static DEFINE_PER_CPU(int, irqs_off_counting);
#endif

#if defined(CONFIG_PREEMPT_ONOFF_TRACER)
static DEFINE_PER_CPU(int, preempt_off_counting);
#endif

void preemptirqs_onoff_helper(enum preemptirqs_onoff_action action)
{
	int cpu = raw_smp_processor_id();

	switch (action) {
#if defined(CONFIG_IRQS_ONOFF_TRACER)
	case IRQS_ON:
		if (per_cpu(irqs_off_counting, cpu)) {
			trace_irqson(cpu);
#if defined(CONFIG_PREEMPT_ONOFF_TRACER)
			if (per_cpu(preempt_off_counting, cpu))
				trace_preemptorirqson(cpu);
#endif
			per_cpu(irqs_off_counting, cpu) = 0;
		}
		break;
	case IRQS_OFF:
		if (!per_cpu(irqs_off_counting, cpu)) {
			trace_irqsoff(cpu);
#if defined(CONFIG_PREEMPT_ONOFF_TRACER)
			if (per_cpu(preempt_off_counting, cpu))
				trace_preemptandirqsoff(cpu);
#endif
			per_cpu(irqs_off_counting, cpu) = 1;
		}
		break;
#endif
#if defined(CONFIG_PREEMPT_ONOFF_TRACER)
	case PREEMPT_ON:
		if (per_cpu(preempt_off_counting, cpu)) {
			trace_preempton(cpu);
			per_cpu(preempt_off_counting, cpu) = 0;
		}
		break;
	case PREEMPT_OFF:
		if (!per_cpu(preempt_off_counting, cpu)) {
			trace_preemptoff(cpu);
			per_cpu(preempt_off_counting, cpu) = 1;
		}
		break;
#endif
	default:
		return;
	}
}
