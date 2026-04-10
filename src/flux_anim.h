#ifndef FLUX_ANIM_H
#define FLUX_ANIM_H

#include "fluxent/flux_types.h"
#include "fluxent/flux_render_cache.h"
#include <math.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  Duration constants (seconds)                                       */
/* ------------------------------------------------------------------ */

#define FLUX_ANIM_DURATION_FAST     0.083   /* ~83ms  — check transitions      */
#define FLUX_ANIM_DURATION_NORMAL   0.167   /* ~167ms — hover / focus           */
#define FLUX_ANIM_DURATION_SLOW     0.250   /* ~250ms — color cross-fades       */
#define FLUX_ANIM_DURATION_PRESS    0.110   /* ~110ms — press (snappier)        */
#define FLUX_ANIM_DURATION_SLIDER   0.083   /* ~83ms  — slider thumb scale      */

#define FLUX_ANIM_EPS  0.001f

/* ------------------------------------------------------------------ */
/*  Core utilities                                                     */
/* ------------------------------------------------------------------ */

/* Linear interpolation */
static inline float flux_anim_mixf(float a, float b, float t) {
    return a + (b - a) * t;
}

/* Clamp t to [0,1] then mix */
static inline float flux_anim_mixf_clamped(float a, float b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return a + (b - a) * t;
}

/* ------------------------------------------------------------------ */
/*  Color mixing (linear RGB, premultiplied-safe)                      */
/* ------------------------------------------------------------------ */

static inline FluxColor flux_anim_lerp_color(FluxColor c0, FluxColor c1, float t) {
    if (t <= 0.0f) return c0;
    if (t >= 1.0f) return c1;
    float r = flux_color_rf(c0) + (flux_color_rf(c1) - flux_color_rf(c0)) * t;
    float g = flux_color_gf(c0) + (flux_color_gf(c1) - flux_color_gf(c0)) * t;
    float b = flux_color_bf(c0) + (flux_color_bf(c1) - flux_color_bf(c0)) * t;
    float a = flux_color_af(c0) + (flux_color_af(c1) - flux_color_af(c0)) * t;
    return flux_color_rgba((uint8_t)(r * 255.0f + 0.5f),
                           (uint8_t)(g * 255.0f + 0.5f),
                           (uint8_t)(b * 255.0f + 0.5f),
                           (uint8_t)(a * 255.0f + 0.5f));
}

/* ------------------------------------------------------------------ */
/*  Float tween                                                        */
/* ------------------------------------------------------------------ */

/*
 * Drive a FluxTween towards new_target over duration seconds.
 * Writes the interpolated value to *out.
 * Returns true if the tween is still in flight (caller needs another frame).
 */
static inline bool flux_tween_update(FluxTween *tw, float new_target,
                                     double duration, double now, float *out) {
    /* First call — snap to target, no animation */
    if (!tw->initialized) {
        tw->current     = new_target;
        tw->target      = new_target;
        tw->start_val   = new_target;
        tw->active      = false;
        tw->initialized = true;
        *out = new_target;
        return false;
    }

    /* Target changed — start a new tween from where we are */
    if (fabsf(new_target - tw->target) > FLUX_ANIM_EPS) {
        tw->start_val  = tw->current;
        tw->target     = new_target;
        tw->start_time = now;
        tw->duration   = duration;
        tw->active     = true;
    }

    /* Tick the active tween */
    if (tw->active) {
        double elapsed = now - tw->start_time;
        double d = tw->duration > 0.0 ? tw->duration : 1.0;
        double t = elapsed / d;
        if (t < 0.0) t = 0.0;
        if (t >= 1.0) {
            tw->current = tw->target;
            tw->active  = false;
        } else {
            tw->current = tw->start_val + (tw->target - tw->start_val) * (float)t;
        }
    }

    *out = tw->current;
    return tw->active;
}

/* ------------------------------------------------------------------ */
/*  Color tween                                                        */
/* ------------------------------------------------------------------ */

/*
 * Drive a FluxColorTween towards new_target over duration seconds.
 * Writes the interpolated color to *out.
 * Returns true if the tween is still in flight.
 *
 * FluxColorTween stores colors as raw uint32_t (rgba packed).
 */
static inline bool flux_color_tween_update(FluxColorTween *tw, FluxColor new_target,
                                           double duration, double now, FluxColor *out) {
    /* First call — snap to target, no animation */
    if (!tw->initialized) {
        tw->current_rgba = new_target.rgba;
        tw->target_rgba  = new_target.rgba;
        tw->start_rgba   = new_target.rgba;
        tw->active       = false;
        tw->initialized  = true;
        *out = new_target;
        return false;
    }

    /* Target changed — start a new tween from where we are */
    if (tw->target_rgba != new_target.rgba) {
        tw->start_rgba = tw->current_rgba;
        tw->target_rgba  = new_target.rgba;
        tw->start_time   = now;
        tw->duration     = duration;
        tw->active       = true;
    }

    /* Tick the active tween */
    if (tw->active) {
        double elapsed = now - tw->start_time;
        double d = tw->duration > 0.0 ? tw->duration : 1.0;
        double t = elapsed / d;
        if (t < 0.0) t = 0.0;
        if (t >= 1.0) {
            tw->current_rgba = tw->target_rgba;
            tw->active = false;
        } else {
            FluxColor sc = { tw->start_rgba };
            FluxColor tc = { tw->target_rgba };
            FluxColor ic = flux_anim_lerp_color(sc, tc, (float)t);
            tw->current_rgba = ic.rgba;
        }
    }

    FluxColor c = { tw->current_rgba };
    *out = c;
    return tw->active;
}

/* ------------------------------------------------------------------ */
/*  Batch state update                                                 */
/* ------------------------------------------------------------------ */

/*
 * Update the four standard animation channels for a control.
 * Each channel is tweened towards 0 or 1 based on the boolean state.
 * Returns true if ANY channel is still animating (caller should
 * request another render frame).
 */
static inline bool flux_tween_update_states(FluxTween *hover, FluxTween *press,
                                            FluxTween *focus, FluxTween *check,
                                            bool is_hovered, bool is_pressed,
                                            bool is_focused, bool is_checked,
                                            double now) {
    bool active = false;
    float v;
    active |= flux_tween_update(hover, is_hovered ? 1.0f : 0.0f,
                                FLUX_ANIM_DURATION_NORMAL, now, &v);
    active |= flux_tween_update(press, is_pressed ? 1.0f : 0.0f,
                                FLUX_ANIM_DURATION_PRESS, now, &v);
    active |= flux_tween_update(focus, is_focused ? 1.0f : 0.0f,
                                FLUX_ANIM_DURATION_NORMAL, now, &v);
    active |= flux_tween_update(check, is_checked ? 1.0f : 0.0f,
                                FLUX_ANIM_DURATION_FAST, now, &v);
    return active;
}

/* ------------------------------------------------------------------ */
/*  Easing helpers (optional — controls can use linear or eased)       */
/* ------------------------------------------------------------------ */

/* Quadratic ease-out: fast start, gentle end */
static inline float flux_ease_out_quad(float t) {
    return t * (2.0f - t);
}

/* Cubic ease-in-out: smooth both ends */
static inline float flux_ease_in_out_cubic(float t) {
    if (t < 0.5f)
        return 4.0f * t * t * t;
    float f = 2.0f * t - 2.0f;
    return 0.5f * f * f * f + 1.0f;
}

#endif /* FLUX_ANIM_H */