#include "../flux_render_internal.h"
#include <math.h>

void flux_draw_slider(const FluxRenderContext *rc,
                      const FluxRenderSnapshot *snap,
                      const FluxRect *bounds,
                      const FluxControlState *state) {
    const FluxThemeColors *t = rc->theme;

    float track_h = 4.0f;
    float thumb_outer = 10.0f;

    float range = snap->max_value - snap->min_value;
    float pct = (range > 0.0f) ? (snap->current_value - snap->min_value) / range : 0.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    float cy = bounds->y + bounds->h * 0.5f;
    float pad = thumb_outer;
    float track_left = bounds->x + pad;
    float track_w = bounds->w - pad * 2.0f;
    if (track_w < 0.0f) track_w = 0.0f;

    /* Track background — uses StrongFill, not AltFill */
    FluxColor track_bg = t ? t->ctrl_strong_fill_default : flux_color_rgba(0, 0, 0, 0x9C);

    /* Determine value fill, inner thumb color based on interaction state */
    FluxColor val_color, inner_c;
    if (!state->enabled) {
        track_bg = t ? t->ctrl_strong_fill_disabled : flux_color_rgba(0, 0, 0, 0x37);
        val_color = t ? t->accent_disabled : flux_color_rgba(0, 0, 0, 0x37);
        inner_c = t ? t->accent_disabled : flux_color_rgba(0, 120, 212, 100);
    } else if (state->pressed) {
        val_color = t ? t->accent_tertiary : flux_color_rgba(0, 120, 212, 0xCC);
        inner_c = t ? t->accent_tertiary : flux_color_rgba(0, 120, 212, 0xCC);
    } else if (state->hovered) {
        val_color = t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xE6);
        inner_c = t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xE6);
    } else {
        val_color = t ? t->accent_default : flux_color_rgb(0, 120, 212);
        inner_c = t ? t->accent_default : flux_color_rgb(0, 120, 212);
    }

    /* Draw track background */
    FluxRect track = { track_left, cy - track_h * 0.5f, track_w, track_h };
    flux_fill_rounded_rect(rc, &track, track_h * 0.5f, track_bg);

    /* Draw value fill */
    float val_w = track_w * pct;
    if (val_w > 0.0f) {
        FluxRect val_r = { track_left, cy - track_h * 0.5f, val_w, track_h };
        flux_fill_rounded_rect(rc, &val_r, track_h * 0.5f, val_color);
    }

    /* Thumb outer circle */
    float thumb_x = track_left + track_w * pct;
    FluxColor thumb_fill = t ? t->ctrl_solid_fill_default : flux_color_rgb(255, 255, 255);
    FluxColor thumb_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0F);
    if (!state->enabled) {
        thumb_stroke = t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37);
    }
    flux_fill_ellipse(rc, thumb_x, cy, thumb_outer, thumb_outer, thumb_fill);
    flux_stroke_ellipse(rc, thumb_x, cy, thumb_outer, thumb_outer, thumb_stroke, 1.0f);

    /* Inner thumb — scale-based radius (kSliderInnerThumbSize=12, radius=6*scale) */
    float target_scale = state->pressed ? 0.71f : (state->hovered ? 1.0f : 0.86f);
    float current_scale = target_scale;
    FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
    if (ce) {
        float scale;
        bool animating = flux_tween_update(&ce->slider_anim, target_scale,
                                           FLUX_ANIM_DURATION_SLIDER, rc->now, &scale);
        if (animating && rc->animations_active)
            *rc->animations_active = true;
        current_scale = scale;
    }
    float ir = 6.0f * current_scale;
    flux_fill_ellipse(rc, thumb_x, cy, ir, ir, inner_c);
}