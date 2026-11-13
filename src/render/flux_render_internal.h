#ifndef FLUX_RENDER_INTERNAL_H
#define FLUX_RENDER_INTERNAL_H

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <cd2d.h>
#include <stdbool.h>

#include "render/flux_engine.h"
#include "controls/factory/flux_control_registry.h"
#include "render/flux_render_snapshot.h"
#include "flux_render_cache.h"
#include "fluxent/flux_text.h"
#include "fluxent/flux_theme.h"
#include "flux_anim.h"

#define FLUX_RENDER_MAX_TRANSFORM_DEPTH      32

#define FLUX_RENDER_COMMAND_INITIAL_CAPACITY 256

typedef struct FluxFillSink {
	FluxColor color;
	float     corner_radius;
	bool      written;
} FluxFillSink;

void flux_engine_bind(FluxEngine *eng, FluxControlRegistry const *registry, XtkBackend *backend);

struct FluxRenderCtx {
	ID2D1DeviceContext    *d2d;
	ID2D1SolidColorBrush  *brush;
	FluxRenderCache       *cache;
	FluxText              *text;
	FluxDpi                dpi;
	FluxThemeColors const *theme;
	double                 now;
	float                  dt;
	bool                  *animations_active;
	bool                   is_dark;
	FluxFillSink          *fill_sink;
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

static void inline flux_set_brush(FluxRenderCtx const *rc, FluxColor c) {
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

static void inline flux_fill_rounded_rect(FluxRenderCtx const *rc, FluxRect const *r, float radius, FluxColor color) {
	D2D1_ROUNDED_RECT  rr = flux_rounded_rect(r, radius);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_FillRoundedRectangle(rt, &rr, ( ID2D1Brush * ) rc->brush);
}

static void inline flux_draw_rounded_rect(
  FluxRenderCtx const *rc, FluxRect const *r, float radius, FluxColor color, float width
) {
	D2D1_ROUNDED_RECT  rr = flux_rounded_rect(r, radius);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_DrawRoundedRectangle(rt, &rr, ( ID2D1Brush * ) rc->brush, width, NULL);
}

static void inline flux_fill_rect(FluxRenderCtx const *rc, FluxRect const *r, FluxColor color) {
	D2D1_RECT_F        dr = flux_d2d_rect(r);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_FillRectangle(rt, &dr, ( ID2D1Brush * ) rc->brush);
}

static void inline flux_fill_ellipse(FluxRenderCtx const *rc, FluxEllipseSpec const *spec, FluxColor color) {
	D2D1_ELLIPSE       e  = flux_ellipse(spec);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_FillEllipse(rt, &e, ( ID2D1Brush * ) rc->brush);
}

static void inline flux_stroke_ellipse(
  FluxRenderCtx const *rc, FluxEllipseSpec const *spec, FluxColor color, float width
) {
	D2D1_ELLIPSE       e  = flux_ellipse(spec);
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_DrawEllipse(rt, &e, ( ID2D1Brush * ) rc->brush, width, NULL);
}

static void inline flux_draw_line(FluxRenderCtx const *rc, FluxLineSpec const *line, FluxColor color, float width) {
	ID2D1RenderTarget *rt = FLUX_RT(rc);
	flux_set_brush(rc, color);
	ID2D1RenderTarget_DrawLine(
	  rt, flux_point(line->x0, line->y0), flux_point(line->x1, line->y1), ( ID2D1Brush * ) rc->brush, width, NULL
	);
}

#endif
