#ifndef FLUX_ANIM_H
#define FLUX_ANIM_H

#include "fluxent/flux_types.h"
#include "flux_render_cache.h"
#include <math.h>
#include <stdbool.h>

#define FLUX_ANIM_DURATION_FAST   0.083
#define FLUX_ANIM_DURATION_NORMAL 0.167
#define FLUX_ANIM_DURATION_SLOW   0.250
#define FLUX_ANIM_DURATION_PRESS  0.110

#define FLUX_ANIM_EPS             0.001f

static float inline flux_srgb_to_linear(float c) {
	if (c <= 0.04045f) return c / 12.92f;
	return powf((c + 0.055f) / 1.055f, 2.4f);
}

static float inline flux_linear_to_srgb(float c) {
	if (c <= 0.0031308f) return 12.92f * c;
	return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

static float inline flux_anim_mixf(float a, float b, float t) { return a + (b - a) * t; }

static uint8_t inline flux_quantize_byte(float v) {
	if (v <= 0.0f) return 0;
	if (v >= 1.0f) return 255;
	return ( uint8_t ) (v * 255.0f + 0.5f);
}

static FluxColor inline flux_anim_lerp_color(FluxColor c0, FluxColor c1, float t) {
	if (t <= 0.0f) return c0;
	if (t >= 1.0f) return c1;

	float a0 = flux_color_af(c0);
	float a1 = flux_color_af(c1);
	float la = flux_anim_mixf(a0, a1, t);

	float pr
	  = flux_anim_mixf(flux_srgb_to_linear(flux_color_rf(c0)) * a0, flux_srgb_to_linear(flux_color_rf(c1)) * a1, t);
	float pg
	  = flux_anim_mixf(flux_srgb_to_linear(flux_color_gf(c0)) * a0, flux_srgb_to_linear(flux_color_gf(c1)) * a1, t);
	float pb
	  = flux_anim_mixf(flux_srgb_to_linear(flux_color_bf(c0)) * a0, flux_srgb_to_linear(flux_color_bf(c1)) * a1, t);

	float inv = la > 0.0001f ? 1.0f / la : 0.0f;
	return flux_color_rgba(
	  flux_quantize_byte(flux_linear_to_srgb(pr * inv)), flux_quantize_byte(flux_linear_to_srgb(pg * inv)),
	  flux_quantize_byte(flux_linear_to_srgb(pb * inv)), flux_quantize_byte(la)
	);
}

float flux_cubic_bezier(float x, float x1, float y1, float x2, float y2);

static float inline flux_ease_out_quad(float t) { return t * (2.0f - t); }

static float inline flux_ease_out_cubic(float t) {
	float u = 1.0f - t;
	return 1.0f - u * u * u;
}

static bool inline flux_tween_update(FluxTween *tw, float new_target, double duration, double now, float *out) {
	if (!tw->initialized) {
		tw->current     = new_target;
		tw->target      = new_target;
		tw->start_val   = new_target;
		tw->active      = false;
		tw->initialized = true;
		*out            = new_target;
		return false;
	}

	if (fabsf(new_target - tw->target) > FLUX_ANIM_EPS) {
		tw->start_val  = tw->current;
		tw->target     = new_target;
		tw->start_time = now;
		tw->duration   = duration;
		tw->active     = true;
	}

	if (tw->active) {
		double elapsed = now - tw->start_time;
		double d       = tw->duration > 0.0 ? tw->duration : 1.0;
		double t       = elapsed / d;
		if (t < 0.0) t = 0.0;
		if (t >= 1.0) {
			tw->current = tw->target;
			tw->active  = false;
		}
		else {
			float (*easing)(float) = tw->easing ? tw->easing : flux_ease_out_quad;
			float eased            = easing(( float ) t);
			tw->current            = tw->start_val + (tw->target - tw->start_val) * eased;
		}
	}

	*out = tw->current;
	return tw->active;
}

static bool inline flux_color_tween_update(
  FluxColorTween *tw, FluxColor new_target, double duration, double now, FluxColor *out
) {
	if (!tw->initialized) {
		tw->current_rgba = new_target.rgba;
		tw->target_rgba  = new_target.rgba;
		tw->start_rgba   = new_target.rgba;
		tw->active       = false;
		tw->initialized  = true;
		*out             = new_target;
		return false;
	}

	if (tw->target_rgba != new_target.rgba) {
		tw->start_rgba  = tw->current_rgba;
		tw->target_rgba = new_target.rgba;
		tw->start_time  = now;
		tw->duration    = duration;
		tw->active      = true;
	}

	if (tw->active) {
		double elapsed = now - tw->start_time;
		double d       = tw->duration > 0.0 ? tw->duration : 1.0;
		double t       = elapsed / d;
		if (t < 0.0) t = 0.0;
		if (t >= 1.0) {
			tw->current_rgba = tw->target_rgba;
			tw->active       = false;
		}
		else {
			float     (*easing)(float) = tw->easing ? tw->easing : flux_ease_out_quad;
			FluxColor sc               = {tw->start_rgba};
			FluxColor tc               = {tw->target_rgba};
			FluxColor ic               = flux_anim_lerp_color(sc, tc, easing(( float ) t));
			tw->current_rgba           = ic.rgba;
		}
	}

	FluxColor c = {tw->current_rgba};
	*out        = c;
	return tw->active;
}

typedef struct FluxTweenChannels {
	FluxTween *hover;
	FluxTween *press;
	FluxTween *focus;
	FluxTween *check;
} FluxTweenChannels;

typedef struct FluxTweenTargets {
	bool hover;
	bool press;
	bool focus;
	bool check;
} FluxTweenTargets;

static bool inline flux_tween_update_states(FluxTweenChannels const *channels, FluxTweenTargets targets, double now) {
	bool  active = false;
	float v;
	active |= flux_tween_update(channels->hover, targets.hover ? 1.0f : 0.0f, FLUX_ANIM_DURATION_NORMAL, now, &v);
	active |= flux_tween_update(channels->press, targets.press ? 1.0f : 0.0f, FLUX_ANIM_DURATION_PRESS, now, &v);
	active |= flux_tween_update(channels->focus, targets.focus ? 1.0f : 0.0f, FLUX_ANIM_DURATION_NORMAL, now, &v);
	active |= flux_tween_update(channels->check, targets.check ? 1.0f : 0.0f, FLUX_ANIM_DURATION_FAST, now, &v);
	return active;
}

#endif
