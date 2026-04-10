#include "../flux_render_internal.h"

void flux_draw_progress(const FluxRenderContext *rc,
                        const FluxRenderSnapshot *snap,
                        const FluxRect *bounds,
                        const FluxControlState *state) {
    (void)state;
    const FluxThemeColors *t = rc->theme;

    float track_h = 4.0f;
    float radius = track_h * 0.5f;
    float cy = bounds->y + bounds->h * 0.5f;

    FluxRect track = { bounds->x, cy - track_h * 0.5f, bounds->w, track_h };
    FluxColor track_color = t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 30);
    flux_fill_rounded_rect(rc, &track, radius, track_color);

    float max_val = snap->max_value > 0.0f ? snap->max_value : 1.0f;
    float pct = snap->current_value / max_val;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    float fill_w = bounds->w * pct;
    if (fill_w > 0.0f) {
        FluxRect fill = { bounds->x, cy - track_h * 0.5f, fill_w, track_h };
        FluxColor accent = t ? t->accent_default : flux_color_rgb(0, 120, 212);
        flux_fill_rounded_rect(rc, &fill, radius, accent);
    }
}