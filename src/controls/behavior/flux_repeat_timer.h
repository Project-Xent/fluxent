#ifndef FLUX_REPEAT_TIMER_H
#define FLUX_REPEAT_TIMER_H

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

typedef struct FluxRepeatTimer {
	UINT_PTR                id;
	uint32_t                interval_ms;
	void                    (*tick)(void *ctx);
	void                   *ctx;
	struct FluxRepeatTimer *next;
} FluxRepeatTimer;

void flux_repeat_timer_start(
  FluxRepeatTimer *timer, uint32_t delay_ms, uint32_t interval_ms, void (*tick)(void *ctx), void *ctx
);

void flux_repeat_timer_stop(FluxRepeatTimer *timer);

bool flux_repeat_timer_running(FluxRepeatTimer const *timer);

#endif
