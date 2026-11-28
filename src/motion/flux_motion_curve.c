#include "motion/flux_motion_internal.h"
#include "render/flux_anim.h"

#include <math.h>
#include <stdlib.h>

float flux_motion_bezier(float t, float x1, float y1, float x2, float y2) {
	return flux_cubic_bezier(t, x1, y1, x2, y2);
}

float flux_motion_exp_in(float t, float exponent) {
	if (t <= 0.0f) return 0.0f;
	if (t >= 1.0f) return 1.0f;
	if (fabsf(exponent) < 1e-4f) return t;
	return (expf(exponent * t) - 1.0f) / (expf(exponent) - 1.0f);
}

float flux_motion_exp_out(float t, float exponent) {
	if (t <= 0.0f) return 0.0f;
	if (t >= 1.0f) return 1.0f;
	return 1.0f - flux_motion_exp_in(1.0f - t, exponent);
}

float flux_motion_seg(double elapsed, double begin, double dur, float x1, float y1, float x2, float y2) {
	if (elapsed <= begin) return 0.0f;
	if (dur <= 0.0 || elapsed >= begin + dur) return 1.0f;
	float t = ( float ) ((elapsed - begin) / dur);
	return flux_cubic_bezier(t, x1, y1, x2, y2);
}

float     flux_motion_lerpf(float a, float b, float t) { return a + (b - a) * t; }

FluxColor flux_motion_lerp_rgb(FluxColor a, FluxColor b, float t) {
	if (t <= 0.0f) return a;
	if (t >= 1.0f) return b;
	return flux_anim_lerp_color(a, b, t);
}

static bool float_same(float a, float b) { return fabsf(a - b) < 1e-4f; }

static bool color_same(FluxColor a, FluxColor b) { return a.rgba == b.rgba; }

void        flux_motion_tween_set(
  FluxMotionTween *tw, float target, double now, double dur, float x1, float y1, float x2, float y2, bool animate
) {
	if (!tw->initialized || !animate || dur <= 0.0) {
		tw->start = tw->current = tw->target = target;
		tw->initialized                      = true;
		tw->active                           = false;
		return;
	}
	if (float_same(tw->target, target)) return;
	tw->start      = tw->current;
	tw->target     = target;
	tw->start_time = now;
	tw->duration   = dur;
	tw->x1         = x1;
	tw->y1         = y1;
	tw->x2         = x2;
	tw->y2         = y2;
	tw->active     = !float_same(tw->start, tw->target);
	if (!tw->active) tw->current = target;
}

void flux_motion_color_set(FluxMotionColorTween *tw, FluxColor target, double now, double dur, bool animate) {
	if (!tw->initialized || !animate || dur <= 0.0) {
		tw->start = tw->current = tw->target = target;
		tw->initialized                      = true;
		tw->active                           = false;
		return;
	}
	if (color_same(tw->target, target)) return;
	tw->start      = tw->current;
	tw->target     = target;
	tw->start_time = now;
	tw->duration   = dur;
	tw->active     = !color_same(tw->start, tw->target);
	if (!tw->active) tw->current = target;
}

bool flux_motion_tween_step(FluxMotionTween *tw, double now) {
	if (!tw->active) return false;
	double elapsed = now - tw->start_time;
	if (elapsed <= 0.0) {
		tw->current = tw->start;
		return true;
	}
	if (tw->duration <= 0.0 || elapsed >= tw->duration) {
		tw->current = tw->target;
		tw->active  = false;
		return false;
	}
	float p     = flux_cubic_bezier(( float ) (elapsed / tw->duration), tw->x1, tw->y1, tw->x2, tw->y2);
	tw->current = flux_motion_lerpf(tw->start, tw->target, p);
	return true;
}

bool flux_motion_color_step(FluxMotionColorTween *tw, double now) {
	if (!tw->active) return false;
	double elapsed = now - tw->start_time;
	if (elapsed <= 0.0) {
		tw->current = tw->start;
		return true;
	}
	if (tw->duration <= 0.0 || elapsed >= tw->duration) {
		tw->current = tw->target;
		tw->active  = false;
		return false;
	}
	tw->current = flux_motion_lerp_rgb(tw->start, tw->target, ( float ) (elapsed / tw->duration));
	return true;
}
