#undef TRACE_SYSTEM
#define TRACE_SYSTEM preemptirqs_onoff

#if !defined(_TRACE_PREEMPTIRQS_ONOFF_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PREEMPTIRQS_ONOFF_H

#include <linux/tracepoint.h>

#if defined(CONFIG_IRQS_ONOFF_TRACER)
TRACE_EVENT(irqson,

	TP_PROTO(int cpu_id),

	TP_ARGS(cpu_id),

	TP_STRUCT__entry(
		__field(int,	cpu_id)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
	),

	TP_printk("cpu# %d", __entry->cpu_id)
);

TRACE_EVENT(irqsoff,

	TP_PROTO(int cpu_id),

	TP_ARGS(cpu_id),

	TP_STRUCT__entry(
		__field(int,	cpu_id)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
	),

	TP_printk("cpu# %d", __entry->cpu_id)
);
#endif

#if defined(CONFIG_PREEMPT_ONOFF_TRACER)
TRACE_EVENT(preempton,

	TP_PROTO(int cpu_id),

	TP_ARGS(cpu_id),

	TP_STRUCT__entry(
		__field(int,	cpu_id)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
	),

	TP_printk("cpu# %d", __entry->cpu_id)
);

TRACE_EVENT(preemptoff,

	TP_PROTO(int cpu_id),

	TP_ARGS(cpu_id),

	TP_STRUCT__entry(
		__field(int,	cpu_id)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
	),

	TP_printk("cpu# %d", __entry->cpu_id)
);
#endif

#if defined(CONFIG_IRQS_ONOFF_TRACER) && defined(CONFIG_PREEMPT_ONOFF_TRACER)
TRACE_EVENT(preemptorirqson,

	TP_PROTO(int cpu_id),

	TP_ARGS(cpu_id),

	TP_STRUCT__entry(
		__field(int,	cpu_id)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
	),

	TP_printk("cpu# %d", __entry->cpu_id)
);

TRACE_EVENT(preemptandirqsoff,

	TP_PROTO(int cpu_id),

	TP_ARGS(cpu_id),

	TP_STRUCT__entry(
		__field(int,	cpu_id)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
	),

	TP_printk("cpu# %d", __entry->cpu_id)
);
#endif

#endif /*  _TRACE_PREEMPTIRQS_ONOFF_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
