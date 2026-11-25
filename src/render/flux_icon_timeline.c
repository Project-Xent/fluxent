#include "render/flux_icon_timeline.h"

#include <string.h>

static FluxIconSegment icon_segment(FluxIconTimeline const *t, uint8_t from, uint8_t to) {
	FluxIconSegment fallback = {0};
	bool            found    = false;
	for (size_t i = 0; i < t->count; ++i) {
		FluxIconSegment s = t->segments [i];
		if (s.to != to) continue;
		if (s.from == from) return s;
		if (found && s.from >= fallback.from) continue;
		fallback = (FluxIconSegment) {s.from, to, s.end, s.end};
		found    = true;
	}
	return fallback;
}

static void icon_start(FluxIconTimeline const *t, FluxIconPlayback *p, uint8_t state, double now, bool animate) {
	FluxIconSegment s = icon_segment(t, p->current, state);
	p->started_at     = now;
	p->start_frame    = animate && s.end - s.start >= t->frames_per_second * 0.02f ? s.start : s.end;
	p->end_frame      = s.end;
	p->current        = state;
}

static void icon_next(FluxIconTimeline const *t, FluxIconPlayback *p, double now) {
	icon_start(t, p, p->queued [0], now, true);
	--p->queue_count;
	memmove(p->queued, p->queued + 1, p->queue_count);
}

static void icon_advance(FluxIconTimeline const *t, FluxIconPlayback *p, double now) {
	while (p->queue_count > 0) {
		double end_at = p->started_at + (p->end_frame - p->start_frame) / t->frames_per_second;
		if (now < end_at) break;
		icon_next(t, p, end_at);
	}
}

static void icon_queue_state(FluxIconTimeline const *t, FluxIconPlayback *p, uint8_t state, double now) {
	uint8_t pending = p->queue_count ? p->queued [p->queue_count - 1] : p->current;
	if (state == pending) return;
	double end_at = p->started_at + (p->end_frame - p->start_frame) / t->frames_per_second;
	if (now >= end_at) {
		icon_start(t, p, state, now, true);
		return;
	}
	if (p->queue_count == sizeof(p->queued)) icon_next(t, p, now);
	p->queued [p->queue_count++] = state;
}

float flux_icon_timeline_frame(
  FluxIconTimeline const *t, FluxIconPlayback *p, uint8_t state, double now, bool animate, bool *playing
) {
	*playing = false;
	if (!p->initialized || !animate) {
		p->initialized = true;
		p->queue_count = 0;
		icon_start(t, p, state, now, false);
		return p->end_frame;
	}
	icon_advance(t, p, now);
	icon_queue_state(t, p, state, now);
	float frame = p->start_frame + ( float ) ((now - p->started_at) * t->frames_per_second);
	*playing    = frame < p->end_frame || p->queue_count > 0;
	return frame < p->end_frame ? frame : p->end_frame;
}
