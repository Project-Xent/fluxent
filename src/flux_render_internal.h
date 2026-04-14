#ifndef FLUX_RENDER_INTERNAL_H
#define FLUX_RENDER_INTERNAL_H

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <cd2d.h>
#include <stdbool.h>

#include "fluxent/flux_engine.h"
#include "fluxent/flux_render_snapshot.h"
#include "fluxent/flux_render_cache.h"
#include "fluxent/flux_text.h"
#include "fluxent/flux_theme.h"
#include "flux_anim.h"

struct FluxRenderContext {
    ID2D1DeviceContext   *d2d;
    ID2D1SolidColorBrush *brush;
    FluxRenderCache      *cache;
    FluxTextRenderer     *text;
    FluxDpiInfo           dpi;
    const FluxThemeColors *theme;
    double                now;               /* monotonic time in seconds  */
    float                 dt;                /* seconds since last frame   */
    bool                 *animations_active; /* points to caller's flag; writable through const rc */
    bool                  is_dark;           /* true when dark mode is active */
};

static inline D2D1_COLOR_F flux_d2d_color(FluxColor c) {
    D2D1_COLOR_F d;
    d.r = flux_color_rf(c);
    d.g = flux_color_gf(c);
    d.b = flux_color_bf(c);
    d.a = flux_color_af(c);
    return d;
}

static inline void flux_set_brush(const FluxRenderContext *rc, FluxColor c) {
    D2D1_COLOR_F cf = flux_d2d_color(c);
    ID2D1SolidColorBrush_SetColor(rc->brush, &cf);
}

static inline D2D1_RECT_F flux_d2d_rect(const FluxRect *r) {
    D2D1_RECT_F d;
    d.left   = r->x;
    d.top    = r->y;
    d.right  = r->x + r->w;
    d.bottom = r->y + r->h;
    return d;
}

static inline D2D1_ROUNDED_RECT flux_rounded_rect(const FluxRect *r, float radius) {
    D2D1_ROUNDED_RECT rr;
    rr.rect.left   = r->x;
    rr.rect.top    = r->y;
    rr.rect.right  = r->x + r->w;
    rr.rect.bottom = r->y + r->h;
    rr.radiusX = radius;
    rr.radiusY = radius;
    return rr;
}

static inline D2D1_ROUNDED_RECT flux_rounded_rect_inset(const FluxRect *r, float radius, float inset) {
    D2D1_ROUNDED_RECT rr;
    rr.rect.left   = r->x + inset;
    rr.rect.top    = r->y + inset;
    rr.rect.right  = r->x + r->w - inset;
    rr.rect.bottom = r->y + r->h - inset;
    rr.radiusX = radius > inset ? radius - inset : 0.0f;
    rr.radiusY = radius > inset ? radius - inset : 0.0f;
    return rr;
}

static inline D2D1_ELLIPSE flux_ellipse(float cx, float cy, float rx, float ry) {
    D2D1_ELLIPSE e;
    e.point.x = cx;
    e.point.y = cy;
    e.radiusX = rx;
    e.radiusY = ry;
    return e;
}

static inline D2D1_POINT_2F flux_point(float x, float y) {
    D2D1_POINT_2F p;
    p.x = x;
    p.y = y;
    return p;
}

#define FLUX_RT(rc) ((ID2D1RenderTarget *)(rc)->d2d)

static inline void flux_fill_rounded_rect(const FluxRenderContext *rc, const FluxRect *r,
                                           float radius, FluxColor color) {
    D2D1_ROUNDED_RECT rr = flux_rounded_rect(r, radius);
    flux_set_brush(rc, color);
    ID2D1RenderTarget_FillRoundedRectangle(FLUX_RT(rc), &rr, (ID2D1Brush *)rc->brush);
}

static inline void flux_draw_rounded_rect(const FluxRenderContext *rc, const FluxRect *r,
                                           float radius, FluxColor color, float width) {
    D2D1_ROUNDED_RECT rr = flux_rounded_rect(r, radius);
    flux_set_brush(rc, color);
    ID2D1RenderTarget_DrawRoundedRectangle(FLUX_RT(rc), &rr, (ID2D1Brush *)rc->brush, width, NULL);
}

static inline void flux_fill_rect(const FluxRenderContext *rc, const FluxRect *r, FluxColor color) {
    D2D1_RECT_F dr = flux_d2d_rect(r);
    flux_set_brush(rc, color);
    ID2D1RenderTarget_FillRectangle(FLUX_RT(rc), &dr, (ID2D1Brush *)rc->brush);
}

static inline void flux_fill_ellipse(const FluxRenderContext *rc,
                                      float cx, float cy, float rx, float ry,
                                      FluxColor color) {
    D2D1_ELLIPSE e = flux_ellipse(cx, cy, rx, ry);
    flux_set_brush(rc, color);
    ID2D1RenderTarget_FillEllipse(FLUX_RT(rc), &e, (ID2D1Brush *)rc->brush);
}

static inline void flux_stroke_ellipse(const FluxRenderContext *rc,
                                        float cx, float cy, float rx, float ry,
                                        FluxColor color, float width) {
    D2D1_ELLIPSE e = flux_ellipse(cx, cy, rx, ry);
    flux_set_brush(rc, color);
    ID2D1RenderTarget_DrawEllipse(FLUX_RT(rc), &e, (ID2D1Brush *)rc->brush, width, NULL);
}

static inline void flux_draw_line(const FluxRenderContext *rc,
                                   float x0, float y0, float x1, float y1,
                                   FluxColor color, float width) {
    flux_set_brush(rc, color);
    ID2D1RenderTarget_DrawLine(FLUX_RT(rc), flux_point(x0, y0), flux_point(x1, y1),
                               (ID2D1Brush *)rc->brush, width, NULL);
}

static inline void flux_fill_background(const FluxRenderContext *rc,
                                         const FluxRenderSnapshot *snap,
                                         const FluxRect *bounds) {
    if (flux_color_af(snap->background) <= 0.0f)
        return;
    if (snap->corner_radius > 0.0f)
        flux_fill_rounded_rect(rc, bounds, snap->corner_radius, snap->background);
    else
        flux_fill_rect(rc, bounds, snap->background);
}

static inline void flux_stroke_border(const FluxRenderContext *rc,
                                       const FluxRenderSnapshot *snap,
                                       const FluxRect *bounds) {
    if (snap->border_width <= 0.0f || flux_color_af(snap->border_color) <= 0.0f)
        return;
    float half = snap->border_width * 0.5f;
    FluxRect inset = { bounds->x + half, bounds->y + half,
                       bounds->w - half * 2.0f, bounds->h - half * 2.0f };
    if (snap->corner_radius > 0.0f)
        flux_draw_rounded_rect(rc, &inset, snap->corner_radius, snap->border_color, snap->border_width);
    else {
        D2D1_RECT_F dr = flux_d2d_rect(&inset);
        flux_set_brush(rc, snap->border_color);
        ID2D1RenderTarget_DrawRectangle(FLUX_RT(rc), &dr, (ID2D1Brush *)rc->brush, snap->border_width, NULL);
    }
}

#endif