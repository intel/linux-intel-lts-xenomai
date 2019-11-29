#if !defined(_TRACE_PREEMPTIRQS_ONOFF_HELPER_H)
#define _TRACE_PREEMPTIRQS_ONOFF_HELPER_H

enum preemptirqs_onoff_action {
	IRQS_ON,
	IRQS_OFF,
	PREEMPT_ON,
	PREEMPT_OFF
};

#if !defined(CONFIG_IRQS_ONOFF_TRACER) && !defined(CONFIG_PREEMPT_ONOFF_TRACER)
#define preemptirqs_onoff_helper(a)
#else
void preemptirqs_onoff_helper(enum preemptirqs_onoff_action action);
#endif

#endif /*  _TRACE_PREEMPTIRQS_ONOFF_HELPER_H */
