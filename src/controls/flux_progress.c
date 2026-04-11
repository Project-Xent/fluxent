#include "../flux_render_internal.h"
#include <math.h>

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

    if (snap->indeterminate) {
        /* Indeterminate mode: sliding bar that ping-pongs across the track */
        const double cycle_duration = 2.0;
        double phase = fmod(rc->now, cycle_duration);
        /* Normalize to [0, 1] */
        float linear_t = (float)(phase / cycle_duration);
        /* Ping-pong: map [0,1] -> [0,1,0] */
        float ping_pong = linear_t < 0.5f
            ? linear_t * 2.0f
            : 2.0f - linear_t * 2.0f;
        /* Apply cubic ease-in-out for smooth acceleration/deceleration */
        float eased = flux_ease_in_out_cubic(ping_pong);

        float bar_w = bounds->w * 0.4f;
        float travel = bounds->w - bar_w;
        float bar_x = bounds->x + travel * eased;

        FluxRect fill = { bar_x, cy - track_h * 0.5f, bar_w, track_h };
        FluxColor accent = t ? t->accent_default : flux_color_rgb(0, 120, 212);
        flux_fill_rounded_rect(rc, &fill, radius, accent);

        /* Keep the animation loop running */
        *rc->animations_active = true;
    } else {
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
}