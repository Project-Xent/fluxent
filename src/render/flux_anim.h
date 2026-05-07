/**
 * @file flux_anim.h
 * @brief Animation utilities and easing functions for Fluxent.
 *
 * Provides inline helpers for smooth property transitions:
 * - Linear interpolation (mix)
 * - Easing curves (ease-out, ease-in-out)
 * - Color blending with gamma-correct sRGB
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

/** @brief Linear interpolation between two values. */
static float inline flux_anim_mixf(float a, float b, float t) { return a + (b - a) * t; }

/** @brief Clamped linear interpolation (t clamped to [0,1]). */
static float inline flux_anim_mixf_clamped(float a, float b, float t) {
	if (t <= 0.0f) return a;
	if (t >= 1.0f) return b;
	return a + (b - a) * t;
}

/** @brief Linearly interpolate between two colors. */
static FluxColor inline flux_anim_lerp_color(FluxColor c0, FluxColor c1, float t) {
	if (t <= 0.0f) return c0;
	if (t >= 1.0f) return c1;
	float r = flux_color_rf(c0) + (flux_color_rf(c1) - flux_color_rf(c0)) * t;
	float g = flux_color_gf(c0) + (flux_color_gf(c1) - flux_color_gf(c0)) * t;
	float b = flux_color_bf(c0) + (flux_color_bf(c1) - flux_color_bf(c0)) * t;
	float a = flux_color_af(c0) + (flux_color_af(c1) - flux_color_af(c0)) * t;
	return flux_color_rgba(
	  ( uint8_t ) (r * 255.0f + 0.5f), ( uint8_t ) (g * 255.0f + 0.5f), ( uint8_t ) (b * 255.0f + 0.5f),
	  ( uint8_t ) (a * 255.0f + 0.5f)
	);
}

/**
 * @brief Drive a FluxTween towards a target over a duration.
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
		else { tw->current = tw->start_val + (tw->target - tw->start_val) * ( float ) t; }
	}

	*out = tw->current;
	return tw->active;
}

/**
 * @brief Drive a FluxColorTween towards a target color over a duration.
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
			FluxColor sc     = {tw->start_rgba};
			FluxColor tc     = {tw->target_rgba};
			FluxColor ic     = flux_anim_lerp_color(sc, tc, ( float ) t);
			tw->current_rgba = ic.rgba;
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

/** @brief Update the four standard animation channels for a control. */
static bool inline flux_tween_update_states(FluxTweenChannels const *channels, FluxTweenTargets targets, double now) {
	bool  active = false;
	float v;
	active |= flux_tween_update(channels->hover, targets.hover ? 1.0f : 0.0f, FLUX_ANIM_DURATION_NORMAL, now, &v);
	active |= flux_tween_update(channels->press, targets.press ? 1.0f : 0.0f, FLUX_ANIM_DURATION_PRESS, now, &v);
	active |= flux_tween_update(channels->focus, targets.focus ? 1.0f : 0.0f, FLUX_ANIM_DURATION_NORMAL, now, &v);
	active |= flux_tween_update(channels->check, targets.check ? 1.0f : 0.0f, FLUX_ANIM_DURATION_FAST, now, &v);
	return active;
}

/** @brief Quadratic ease-out: fast start, gentle end. */
static float inline flux_ease_out_quad(float t) { return t * (2.0f - t); }

/** @brief Cubic ease-in-out: smooth acceleration and deceleration. */
static float inline flux_ease_in_out_cubic(float t) {
	if (t < 0.5f) return 4.0f * t * t * t;
	float f = 2.0f * t - 2.0f;
	return 0.5f * f * f * f + 1.0f;
}

#endif /* FLUX_ANIM_H */
