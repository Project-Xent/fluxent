#include "../flux_render_internal.h"

void flux_draw_scroll(const FluxRenderContext *rc,
                      const FluxRenderSnapshot *snap,
                      const FluxRect *bounds,
                      const FluxControlState *state) {
    (void)state;
    flux_fill_background(rc, snap, bounds);
}

void flux_draw_scroll_overlay(const FluxRenderContext *rc,
                              const FluxRenderSnapshot *snap,
                              const FluxRect *bounds) {
    const FluxThemeColors *t = rc->theme;
    float bar_w = 6.0f;
    float bar_pad = 2.0f;
    FluxColor bar_color = t ? t->ctrl_strong_fill_default : flux_color_rgba(0, 0, 0, 60);

    if (snap->scroll_content_h > bounds->h && snap->scroll_content_h > 0.0f) {
        float ratio = bounds->h / snap->scroll_content_h;
        if (ratio > 1.0f) ratio = 1.0f;
        float bar_h = bounds->h * ratio;
        if (bar_h < 20.0f) bar_h = 20.0f;
        float max_scroll = snap->scroll_content_h - bounds->h;
        float pct = (max_scroll > 0.0f) ? snap->scroll_y / max_scroll : 0.0f;
        float bar_y = bounds->y + (bounds->h - bar_h) * pct;
        FluxRect bar = { bounds->x + bounds->w - bar_w - bar_pad, bar_y, bar_w, bar_h };
        flux_fill_rounded_rect(rc, &bar, bar_w * 0.5f, bar_color);
    }

    if (snap->scroll_content_w > bounds->w && snap->scroll_content_w > 0.0f) {
        float ratio = bounds->w / snap->scroll_content_w;
        if (ratio > 1.0f) ratio = 1.0f;
        float bar_h_horiz = bar_w;
        float bar_w_horiz = bounds->w * ratio;
        if (bar_w_horiz < 20.0f) bar_w_horiz = 20.0f;
        float max_scroll = snap->scroll_content_w - bounds->w;
        float pct = (max_scroll > 0.0f) ? snap->scroll_x / max_scroll : 0.0f;
        float bar_x = bounds->x + (bounds->w - bar_w_horiz) * pct;
        FluxRect bar = { bar_x, bounds->y + bounds->h - bar_h_horiz - bar_pad, bar_w_horiz, bar_h_horiz };
        flux_fill_rounded_rect(rc, &bar, bar_h_horiz * 0.5f, bar_color);
    }
}