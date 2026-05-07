/**
 * @file flux_render_internal.h
 * @brief Internal render context and D2D helper utilities.
 *
 * Defines FluxRenderContext passed to control render functions and
 * provides inline helpers for D2D color/rect conversion.
 * @note This is an internal header; do not include from public API.
 */

#ifndef FLUX_RENDER_INTERNAL_H
#define FLUX_RENDER_INTERNAL_H

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <cd2d.h>
#include <stdbool.h>

#include "fluxent/flux_engine.h"
#include "fluxent/flux_render_snapshot.h"
#include "flux_render_cache.h"
#include "fluxent/flux_text.h"
#include "fluxent/flux_theme.h"
#include "flux_anim.h"

/** @brief Render context passed to all control render functions. */
struct FluxRenderContext {
	ID2D1DeviceContext    *d2d;
	ID2D1SolidColorBrush  *brush;
	FluxRenderCache       *cache;
	FluxTextRenderer      *text;
	FluxDpiInfo            dpi;
	FluxThemeColors const *theme;
	double                 now;               /**< Monotonic time in seconds. */
	float                  dt;                /**< Seconds since last frame. */
	bool                  *animations_active; /**< Writable animation flag owned by the caller. */
	bool                   is_dark;           /**< Current theme is dark. */
};

typedef struct FluxEllipseSpec {
	float cx;
	float cy;
	float rx;
	float ry;
} FluxEllipseSpec;

typedef struct FluxLineSpec {
	float x0;
	float y0;
	float x1;
	float y1;
} FluxLineSpec;

static inline D2D1_COLOR_F flux_d2d_color(FluxColor c) {
	D2D1_COLOR_F d;
	d.r = flux_color_rf(c);
	d.g = flux_color_gf(c);
	d.b = flux_color_bf(c);
	d.a = flux_color_af(c);
	return d;
}

static void inline flux_set_brush(FluxRenderContext const *rc, FluxColor c) {
	D2D1_COLOR_F cf = flux_d2d_color(c);
	ID2D1SolidColorBrush_SetColor(rc->brush, &cf);
}

static inline D2D1_RECT_F flux_d2d_rect(FluxRect const *r) {
	D2D1_RECT_F d;
	d.left   = r->x;
	d.top    = r->y;
	d.right  = r->x + r->w;
	d.bottom = r->y + r->h;
	return d;
}

static inline D2D1_ROUNDED_RECT flux_rounded_rect(FluxRect const *r, float radius) {
	D2D1_ROUNDED_RECT rr;
	rr.rect.left   = r->x;
	rr.rect.top    = r->y;
	rr.rect.right  = r->x + r->w;
	rr.rect.bottom = r->y + r->h;
	rr.radiusX     = radius;
	rr.radiusY     = radius;
	return rr;
}

static inline D2D1_ROUNDED_RECT flux_rounded_rect_inset(FluxRect const *r, float radius, float inset) {
	D2D1_ROUNDED_RECT rr;
	rr.rect.left   = r->x + inset;
	rr.rect.top    = r->y + inset;
	rr.rect.right  = r->x + r->w - inset;
	rr.rect.bottom = r->y + r->h - inset;
	rr.radiusX     = radius > inset ? radius - inset : 0.0f;
	rr.radiusY     = radius > inset ? radius - inset : 0.0f;
	return rr;
}

static inline D2D1_ELLIPSE flux_ellipse(FluxEllipseSpec const *spec) {
	D2D1_ELLIPSE e;
	e.point.x = spec->cx;
	e.point.y = spec->cy;
	e.radiusX = spec->rx;
	e.radiusY = spec->ry;
	return e;
}

static inline D2D1_POINT_2F flux_point(float x, float y) {
	D2D1_POINT_2F p;
	p.x = x;
	p.y = y;
	return p;
}

#define FLUX_RT(rc) (( ID2D1RenderTarget * ) (rc)->d2d)

static void inline flux_fill_rounded_rect(
  FluxRenderContext const *rc, FluxRect const *r, float radius, FluxColor color
) {
	D2D1_ROUNDED_RECT  rr = flux_rounded_rect(r, radius);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_FillRoundedRectangle(rt, &rr, ( ID2D1Brush * ) rc->brush);
}

static void inline flux_draw_rounded_rect(
  FluxRenderContext const *rc, FluxRect const *r, float radius, FluxColor color, float width
) {
	D2D1_ROUNDED_RECT  rr = flux_rounded_rect(r, radius);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_DrawRoundedRectangle(rt, &rr, ( ID2D1Brush * ) rc->brush, width, NULL);
}

static void inline flux_fill_rect(FluxRenderContext const *rc, FluxRect const *r, FluxColor color) {
	D2D1_RECT_F        dr = flux_d2d_rect(r);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_FillRectangle(rt, &dr, ( ID2D1Brush * ) rc->brush);
}

static void inline flux_fill_ellipse(FluxRenderContext const *rc, FluxEllipseSpec const *spec, FluxColor color) {
	D2D1_ELLIPSE       e  = flux_ellipse(spec);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_FillEllipse(rt, &e, ( ID2D1Brush * ) rc->brush);
}

static void inline flux_stroke_ellipse(
  FluxRenderContext const *rc, FluxEllipseSpec const *spec, FluxColor color, float width
) {
	D2D1_ELLIPSE       e  = flux_ellipse(spec);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_DrawEllipse(rt, &e, ( ID2D1Brush * ) rc->brush, width, NULL);
}

static void inline flux_draw_line(FluxRenderContext const *rc, FluxLineSpec const *line, FluxColor color, float width) {
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_DrawLine(
	  rt, flux_point(line->x0, line->y0), flux_point(line->x1, line->y1), ( ID2D1Brush * ) rc->brush, width, NULL
	);
}

static void inline flux_fill_background(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds
) {
	if (flux_color_af(snap->background) <= 0.0f) return;
	if (snap->corner_radius > 0.0f) flux_fill_rounded_rect(rc, bounds, snap->corner_radius, snap->background);
	else flux_fill_rect(rc, bounds, snap->background);
}

static void inline flux_stroke_border(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds
) {
	if (snap->border_width <= 0.0f || flux_color_af(snap->border_color) <= 0.0f) return;
	float    half  = snap->border_width * 0.5f;
	FluxRect inset = {bounds->x + half, bounds->y + half, bounds->w - half * 2.0f, bounds->h - half * 2.0f};
	if (snap->corner_radius > 0.0f)
		flux_draw_rounded_rect(rc, &inset, snap->corner_radius, snap->border_color, snap->border_width);
	else {
		D2D1_RECT_F        dr = flux_d2d_rect(&inset);
		ID2D1RenderTarget *rt = FLUX_RT(rc);
		flux_set_brush(rc, snap->border_color);
		ID2D1RenderTarget_DrawRectangle(rt, &dr, ( ID2D1Brush * ) rc->brush, snap->border_width, NULL);
	}
}

#endif
