/**
 * @file flux_anim.h
 * @brief Animation utilities and easing functions for Fluxent.
 *
 * Provides inline helpers for smooth property transitions:
 * - Linear interpolation (mix)
 * - Easing curves (ease-out, ease-in-out, ease-out-cubic, ease-out-quint, ease-in-quart)
 * - Color blending in linear light (gamma-correct sRGB; alpha stays linear)
 * - Progress step functions for state transitions
 *
 * All duration constants follow WinUI 3 timing specifications.
 */

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
#define FLUX_ANIM_DURATION_SLIDER 0.083  /**< ~83ms  — slider thumb scale */

#define FLUX_ANIM_EPS             0.001f /**< Convergence threshold for animations */

/** @brief Decode a single sRGB-encoded channel in [0,1] to linear light. */
static float inline flux_srgb_to_linear(float c) {
	if (c <= 0.04045f) return c / 12.92f;
	return powf((c + 0.055f) / 1.055f, 2.4f);
}

/** @brief Encode a single linear-light channel in [0,1] to sRGB. */
static float inline flux_linear_to_srgb(float c) {
	if (c <= 0.0031308f) return 12.92f * c;
	return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

/** @brief Linear interpolation between two values. */
static float inline flux_anim_mixf(float a, float b, float t) { return a + (b - a) * t; }

/** @brief Clamped linear interpolation (t clamped to [0,1]). */
static float inline flux_anim_mixf_clamped(float a, float b, float t) {
	if (t <= 0.0f) return a;
	if (t >= 1.0f) return b;
	return a + (b - a) * t;
}

static uint8_t inline flux_quantize_unit_to_byte(float v) {
	if (v <= 0.0f) return 0;
	if (v >= 1.0f) return 255;
	return ( uint8_t ) (v * 255.0f + 0.5f);
}

/** @brief Gamma-correct, alpha-correct color lerp: blends in premultiplied linear
 * space, then un-premultiplies back to straight RGBA. Premultiplying is what keeps
 * a transition between colors of differing alpha (e.g. dark mode's faint-white
 * `ctrl_fill_default` -> opaque-dark `ctrl_fill_input_active`) from passing through
 * a bright intermediate -- straight-alpha lerp ramps RGB and alpha independently,
 * so mid-transition you briefly get near-white at rising opacity (a white flash).
 * For equal-alpha or opaque endpoints this reduces to a straight lerp. */
static FluxColor inline flux_anim_lerp_color(FluxColor c0, FluxColor c1, float t) {
	if (t <= 0.0f) return c0;
	if (t >= 1.0f) return c1;

	float a0 = flux_color_af(c0);
	float a1 = flux_color_af(c1);
	float la = flux_anim_mixf(a0, a1, t);

	float pr = flux_anim_mixf(flux_srgb_to_linear(flux_color_rf(c0)) * a0, flux_srgb_to_linear(flux_color_rf(c1)) * a1, t);
	float pg = flux_anim_mixf(flux_srgb_to_linear(flux_color_gf(c0)) * a0, flux_srgb_to_linear(flux_color_gf(c1)) * a1, t);
	float pb = flux_anim_mixf(flux_srgb_to_linear(flux_color_bf(c0)) * a0, flux_srgb_to_linear(flux_color_bf(c1)) * a1, t);

	float inv = la > 0.0001f ? 1.0f / la : 0.0f; /* un-premultiply to straight RGB */
	return flux_color_rgba(
	  flux_quantize_unit_to_byte(flux_linear_to_srgb(pr * inv)), flux_quantize_unit_to_byte(flux_linear_to_srgb(pg * inv)),
	  flux_quantize_unit_to_byte(flux_linear_to_srgb(pb * inv)), flux_quantize_unit_to_byte(la)
	);
}

/**
 * @brief Unit cubic-bezier easing (P0=0, P3=1), Newton-solved.
 *
 * Out-of-line (see flux_anim.c) so the iteration loop is emitted once and shared
 * by all callers rather than inlined per translation unit. Pass the two control
 * points (x1,y1,x2,y2) of a CSS-style cubic-bezier timing curve; `x` is the
 * normalized progress in [0,1] and the return is the eased value.
 */
float flux_cubic_bezier(float x, float x1, float y1, float x2, float y2);

/** @brief Linear easing (identity). */
static float inline flux_ease_linear(float t) { return t; }

/** @brief Quadratic ease-out: fast start, gentle end. */
static float inline flux_ease_out_quad(float t) { return t * (2.0f - t); }

/** @brief Cubic ease-out: WinUI Fluent Move/Show curve. */
static float inline flux_ease_out_cubic(float t) {
	float u = 1.0f - t;
	return 1.0f - u * u * u;
}

/** @brief Cubic ease-in-out: smooth acceleration and deceleration. */
static float inline flux_ease_in_out_cubic(float t) {
	if (t < 0.5f) return 4.0f * t * t * t;
	float f = 2.0f * t - 2.0f;
	return 0.5f * f * f * f + 1.0f;
}

/** @brief Quintic ease-out: WinUI Fluent entrance curve (strong deceleration). */
static float inline flux_ease_out_quint(float t) {
	float u = 1.0f - t;
	return 1.0f - u * u * u * u * u;
}

/** @brief Quartic ease-in: WinUI Fluent exit curve (strong acceleration). */
static float inline flux_ease_in_quart(float t) { return t * t * t * t; }

/**
 * @brief Drive a FluxTween towards a target over a duration.
 *
 * If `tw->easing` is non-NULL, that easing function is applied to the
 * normalized progress before interpolation; otherwise ease-out-quad is
 * used as the default (WinUI-style short transition).
 *
 * @param tw        The tween state to update.
 * @param new_target The target value to animate towards.
 * @param duration  Animation duration in seconds.
 * @param now       Current time in seconds.
 * @param out       Output: the interpolated value.
 * @return true if the tween is still animating (caller needs another frame).
 */
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

/**
 * @brief Drive a FluxColorTween towards a target color over a duration.
 *
 * Color interpolation goes through gamma-correct sRGB blending. Easing is
 * applied via `tw->easing` (default ease-out-quad when NULL).
 *
 * @param tw         The color tween state to update.
 * @param new_target The target color to animate towards.
 * @param duration   Animation duration in seconds.
 * @param now        Current time in seconds.
 * @param out        Output: the interpolated color.
 * @return true if the tween is still animating.
 */
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

/**
 * @brief Update the four standard animation channels for a control.
 *
 * Each channel honors its `easing` field; when unset the per-channel
 * default (ease-out-quad) is used by `flux_tween_update`.
 */
static bool inline flux_tween_update_states(FluxTweenChannels const *channels, FluxTweenTargets targets, double now) {
	bool  active = false;
	float v;
	active |= flux_tween_update(channels->hover, targets.hover ? 1.0f : 0.0f, FLUX_ANIM_DURATION_NORMAL, now, &v);
	active |= flux_tween_update(channels->press, targets.press ? 1.0f : 0.0f, FLUX_ANIM_DURATION_PRESS, now, &v);
	active |= flux_tween_update(channels->focus, targets.focus ? 1.0f : 0.0f, FLUX_ANIM_DURATION_NORMAL, now, &v);
	active |= flux_tween_update(channels->check, targets.check ? 1.0f : 0.0f, FLUX_ANIM_DURATION_FAST, now, &v);
	return active;
}

#endif /* FLUX_ANIM_H */
