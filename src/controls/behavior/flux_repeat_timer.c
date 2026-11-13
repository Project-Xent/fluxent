#include "controls/behavior/flux_repeat_timer.h"

static FluxRepeatTimer *g_repeat_timers = NULL;

static FluxRepeatTimer *repeat_timer_find(UINT_PTR id) {
	for (FluxRepeatTimer *t = g_repeat_timers; t; t = t->next)
		if (t->id == id) return t;
	return NULL;
}

static void repeat_timer_untrack(FluxRepeatTimer *timer) {
	FluxRepeatTimer **link = &g_repeat_timers;
	while (*link && *link != timer) link = &(*link)->next;
	if (*link) *link = timer->next;
	timer->next = NULL;
}

static void CALLBACK repeat_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD elapsed);

static void          repeat_timer_arm(FluxRepeatTimer *timer, uint32_t delay_ms) {
	timer->id = SetTimer(NULL, 0, delay_ms, repeat_timer_proc);
	if (!timer->id) return;
	timer->next     = g_repeat_timers;
	g_repeat_timers = timer;
}

static void CALLBACK repeat_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD elapsed) {
	( void ) hwnd;
	( void ) msg;
	( void ) elapsed;
	FluxRepeatTimer *timer = repeat_timer_find(id);
	if (!timer) return;

	void  (*tick)(void *) = timer->tick;
	void *ctx             = timer->ctx;
	flux_repeat_timer_stop(timer);
	repeat_timer_arm(timer, timer->interval_ms);
	if (tick) tick(ctx);
}

void flux_repeat_timer_start(
  FluxRepeatTimer *timer, uint32_t delay_ms, uint32_t interval_ms, void (*tick)(void *ctx), void *ctx
) {
	if (!timer) return;
	flux_repeat_timer_stop(timer);
	timer->interval_ms = interval_ms;
	timer->tick        = tick;
	timer->ctx         = ctx;
	repeat_timer_arm(timer, delay_ms);
}

void flux_repeat_timer_stop(FluxRepeatTimer *timer) {
	if (!timer || !timer->id) return;
	repeat_timer_untrack(timer);
	KillTimer(NULL, timer->id);
	timer->id = 0;
}

bool flux_repeat_timer_running(FluxRepeatTimer const *timer) { return timer && timer->id != 0; }
