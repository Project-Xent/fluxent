#include "flux_dl_replay.h"
#include "bridge/flux_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "render/flux_image_cache.h"
#include "render/flux_render_internal.h"
#include "render/flux_resource_intern.h"
#include "render/flux_shape_resource.h"

#include <fluxent/flux_text.h>
#include <controls/data/flux_text_data.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FLUX_XTK_BACKEND_ABI  1u
#define FLUX_XTK_RESOURCE_CAP 64u
#define FLUX_XTK_CLIP_CAP     32u

typedef struct FluxBackendResource {
	XentResourceId id;
	char const    *image_path;
} FluxBackendResource;

typedef enum FluxXtkClipKind
{
	FLUX_XTK_CLIP_RECT    = 0,
	FLUX_XTK_CLIP_ELLIPSE = 1,
	FLUX_XTK_CLIP_ROUNDED = 2,
} FluxXtkClipKind;

typedef struct FluxBackendXtkState {
	FluxBackendCtx       bctx;
	XtkHost             *host;
	FluxRenderCtx const *rc;
	FluxBackendResource  resources [FLUX_XTK_RESOURCE_CAP];
	uint32_t             resource_count;
	D2D1_MATRIX_3X2_F    xform_stack [32];
	uint32_t             xform_depth;
	uint8_t              clip_kind [FLUX_XTK_CLIP_CAP];
	ID2D1Geometry       *clip_geometry [FLUX_XTK_CLIP_CAP];
	uint32_t             clip_depth;
	XtkBackend           backend;
} FluxBackendXtkState;

static FluxBackendXtkState *xtk_state(XtkBackend *backend) {
	return backend ? ( FluxBackendXtkState * ) backend->impl : NULL;
}

static FluxColor   xtk_color_to_flux(XentColor c) { return flux_color_rgba(c.r, c.g, c.b, c.a); }

static char const *xtk_resolve_image(FluxBackendXtkState *S, XentResourceId id) {
	if (id == XENT_RESOURCE_INVALID) return NULL;
	for (uint32_t i = 0; i < S->resource_count; ++i)
		if (S->resources [i].id == id) return S->resources [i].image_path;
	return flux_resource_path(id);
}

static FluxRect xent_rect_to_flux(XentRect r) { return (FluxRect) {r.x, r.y, r.w, r.h}; }

static FluxRect xtk_image_dest(FluxRect bounds, float image_w, float image_h, XentImageFit stretch) {
	if (image_w <= 0.0f || image_h <= 0.0f) return bounds;

	float sx    = bounds.w / image_w;
	float sy    = bounds.h / image_h;
	float scale = sx;
	switch (stretch) {
	case XENT_IMAGE_FIT_UNIFORM         : scale = flux_minf(sx, sy); break;
	case XENT_IMAGE_FIT_UNIFORM_TO_FILL : scale = flux_maxf(sx, sy); break;
	case XENT_IMAGE_FIT_NONE            : scale = 1.0f; break;
	default                             : break;
	}

	float w = image_w * scale;
	float h = image_h * scale;
	return (FluxRect) {bounds.x + (bounds.w - w) * 0.5f, bounds.y + (bounds.h - h) * 0.5f, w, h};
}

static void xtk_draw_image(FluxBackendXtkState *S, XentDlCmd const *cmd) {
	if (!S->rc || !S->rc->d2d) return;
	char const *path = xtk_resolve_image(S, cmd->u.image.image);
	if (!path) return;

	float nw = 0.0f, nh = 0.0f;
	void *bmp = flux_image_cache_acquire(S->rc->d2d, path, &nw, &nh);
	if (!bmp) return;

	FluxRect    bounds = xent_rect_to_flux(cmd->u.image.bounds);
	FluxRect    dest   = xtk_image_dest(bounds, nw, nh, cmd->u.image.stretch);
	D2D1_RECT_F d      = {dest.x, dest.y, dest.x + dest.w, dest.y + dest.h};
	ID2D1DeviceContext_DrawBitmap(
	  S->rc->d2d, ( ID2D1Bitmap * ) bmp, &d, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL
	);
}

static void xtk_draw_text(FluxBackendXtkState *S, XentDl const *list, XentDlCmd const *cmd) {
	if (!S->rc || !S->rc->d2d || !S->rc->text) return;
	char const *text = xent_dl_text(list, cmd->u.text.text_offset, cmd->u.text.text_len);
	if (!text || !text [0]) return;

	FluxRect      bounds = xent_rect_to_flux(cmd->u.text.bounds);
	FluxTextStyle style;
	memset(&style, 0, sizeof(style));
	style.font_size   = cmd->u.text.font_size > 0.0f ? cmd->u.text.font_size : 14.0f;
	style.font_weight = cmd->u.text.font_weight;
	style.text_align  = ( FluxTextAlign ) cmd->u.text.h_align;
	style.vert_align  = ( FluxTextVAlign ) cmd->u.text.v_align;
	style.word_wrap   = (cmd->u.text.flags & XENT_DL_TEXT_WRAP) != 0u;
	style.max_lines   = cmd->u.text.max_lines;
	style.trim        = ( XentTextTrim ) cmd->u.text.trim;
	style.color       = xtk_color_to_flux(cmd->u.text.color);
	if (cmd->u.text.font_len > 0u)
		style.font_family = xent_dl_text(list, cmd->u.text.font_offset, cmd->u.text.font_len);
	else if (cmd->u.text.flags & XENT_DL_TEXT_ICON) style.font_family = flux_icon_font_family();
	flux_text_draw(S->rc->text, FLUX_RT(S->rc), text, &bounds, &style);
}

static bool xtk_fill_triangle(FluxBackendXtkState *S, XentDlCmd const *cmd) {
	ID2D1Factory *factory = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(S->rc), &factory);
	if (!factory) return false;
	ID2D1PathGeometry *geo = NULL;
	ID2D1Factory_CreatePathGeometry(factory, &geo);
	ID2D1Factory_Release(factory);
	if (!geo) return false;

	ID2D1GeometrySink *sink = NULL;
	ID2D1PathGeometry_Open(geo, &sink);
	if (!sink) {
		ID2D1PathGeometry_Release(geo);
		return false;
	}
	ID2D1GeometrySink_BeginFigure(sink, flux_point(cmd->u.tri.x0, cmd->u.tri.y0), D2D1_FIGURE_BEGIN_FILLED);
	ID2D1GeometrySink_AddLine(sink, flux_point(cmd->u.tri.x1, cmd->u.tri.y1));
	ID2D1GeometrySink_AddLine(sink, flux_point(cmd->u.tri.x2, cmd->u.tri.y2));
	ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
	ID2D1GeometrySink_Close(sink);
	ID2D1GeometrySink_Release(sink);

	flux_set_brush(S->rc, xtk_color_to_flux(cmd->u.tri.color));
	ID2D1RenderTarget_FillGeometry(FLUX_RT(S->rc), ( ID2D1Geometry * ) geo, ( ID2D1Brush * ) S->rc->brush, NULL);
	ID2D1PathGeometry_Release(geo);
	return true;
}

static ID2D1LinearGradientBrush *xtk_make_linear_brush(
  FluxRenderCtx const *rc, float x0, float y0, float x1, float y1, float s0, float s1, XentColor c0, XentColor c1
) {
	D2D1_GRADIENT_STOP stops [2];
	stops [0].position                      = s0;
	stops [0].color                         = flux_d2d_color(xtk_color_to_flux(c0));
	stops [1].position                      = s1;
	stops [1].color                         = flux_d2d_color(xtk_color_to_flux(c1));

	ID2D1GradientStopCollection *collection = NULL;
	ID2D1RenderTarget_CreateGradientStopCollection(
	  FLUX_RT(rc), stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &collection
	);
	if (!collection) return NULL;

	D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES gp;
	gp.startPoint                  = flux_point(x0, y0);
	gp.endPoint                    = flux_point(x1, y1);
	D2D1_BRUSH_PROPERTIES     bp   = flux_default_brush_props();
	ID2D1LinearGradientBrush *grad = NULL;
	ID2D1RenderTarget_CreateLinearGradientBrush(FLUX_RT(rc), &gp, &bp, collection, &grad);
	ID2D1GradientStopCollection_Release(collection);
	return grad;
}

static bool xtk_stroke_rounded_linear(FluxBackendXtkState *S, XentDlCmd const *cmd) {
	if (cmd->u.linear.stroke_width <= 0.0f) return true;
	ID2D1LinearGradientBrush *grad = xtk_make_linear_brush(
	  S->rc, cmd->u.linear.x0, cmd->u.linear.y0, cmd->u.linear.x1, cmd->u.linear.y1, cmd->u.linear.s0, cmd->u.linear.s1,
	  cmd->u.linear.c0, cmd->u.linear.c1
	);
	if (!grad) return false;
	D2D1_ROUNDED_RECT rr;
	rr.rect.left   = cmd->u.linear.rect.x;
	rr.rect.top    = cmd->u.linear.rect.y;
	rr.rect.right  = cmd->u.linear.rect.x + cmd->u.linear.rect.w;
	rr.rect.bottom = cmd->u.linear.rect.y + cmd->u.linear.rect.h;
	rr.radiusX     = cmd->u.linear.radius;
	rr.radiusY     = cmd->u.linear.radius;
	ID2D1RenderTarget_DrawRoundedRectangle(
	  FLUX_RT(S->rc), &rr, ( ID2D1Brush * ) grad, cmd->u.linear.stroke_width, NULL
	);
	ID2D1LinearGradientBrush_Release(grad);
	return true;
}

static bool xtk_fill_rect_linear(FluxBackendXtkState *S, XentDlCmd const *cmd) {
	ID2D1LinearGradientBrush *grad = xtk_make_linear_brush(
	  S->rc, cmd->u.linear.x0, cmd->u.linear.y0, cmd->u.linear.x1, cmd->u.linear.y1, cmd->u.linear.s0, cmd->u.linear.s1,
	  cmd->u.linear.c0, cmd->u.linear.c1
	);
	if (!grad) return false;
	D2D1_RECT_F r = {
	  cmd->u.linear.rect.x, cmd->u.linear.rect.y, cmd->u.linear.rect.x + cmd->u.linear.rect.w,
	  cmd->u.linear.rect.y + cmd->u.linear.rect.h};
	ID2D1RenderTarget_FillRectangle(FLUX_RT(S->rc), &r, ( ID2D1Brush * ) grad);
	ID2D1LinearGradientBrush_Release(grad);
	return true;
}

static D2D1_ELLIPSE xtk_ellipse(XentRect bounds) {
	return (D2D1_ELLIPSE) {
	  {bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
      bounds.w * 0.5f, bounds.h * 0.5f
    };
}

static D2D1_LAYER_PARAMETERS xtk_layer_parameters(void) {
	D2D1_LAYER_PARAMETERS lp = {0};
	lp.contentBounds         = (D2D1_RECT_F) {-1.0e9f, -1.0e9f, 1.0e9f, 1.0e9f};
	lp.maskAntialiasMode     = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
	lp.maskTransform         = (D2D1_MATRIX_3X2_F) {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
	lp.opacity               = 1.0f;
	return lp;
}

static D2D1_MATRIX_3X2_F xtk_affine_to_d2d(XentAffine2 const *m) {
	return (D2D1_MATRIX_3X2_F) {m->m00, m->m10, m->m01, m->m11, m->m02, m->m12};
}

static D2D1_MATRIX_3X2_F xtk_mul_affine(D2D1_MATRIX_3X2_F a, D2D1_MATRIX_3X2_F b) {
	D2D1_MATRIX_3X2_F o;
	o._11 = a._11 * b._11 + a._12 * b._21;
	o._12 = a._11 * b._12 + a._12 * b._22;
	o._21 = a._21 * b._11 + a._22 * b._21;
	o._22 = a._21 * b._12 + a._22 * b._22;
	o._31 = a._31 * b._11 + a._32 * b._21 + b._31;
	o._32 = a._31 * b._12 + a._32 * b._22 + b._32;
	return o;
}

static bool xtk_push_clip_rect(FluxBackendXtkState *S, XentRect rect) {
	if (S->clip_depth >= FLUX_XTK_CLIP_CAP) return false;
	D2D1_RECT_F clip = flux_d2d_rect(&(FluxRect) {rect.x, rect.y, rect.w, rect.h});
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(S->rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	S->clip_kind [S->clip_depth]      = ( uint8_t ) FLUX_XTK_CLIP_RECT;
	S->clip_geometry [S->clip_depth]  = NULL;
	S->clip_depth                    += 1u;
	return true;
}

static bool xtk_push_clip_ellipse(FluxBackendXtkState *S, XentRect bounds) {
	if (S->clip_depth >= FLUX_XTK_CLIP_CAP) return false;
	ID2D1Factory *factory = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(S->rc), &factory);
	if (!factory) return false;
	D2D1_ELLIPSE          e   = xtk_ellipse(bounds);
	ID2D1EllipseGeometry *geo = NULL;
	HRESULT               hr  = ID2D1Factory_CreateEllipseGeometry(factory, &e, &geo);
	ID2D1Factory_Release(factory);
	if (FAILED(hr) || !geo) return false;
	D2D1_LAYER_PARAMETERS lp = xtk_layer_parameters();
	lp.geometricMask         = ( ID2D1Geometry * ) geo;
	ID2D1RenderTarget_PushLayer(FLUX_RT(S->rc), &lp, NULL);
	S->clip_kind [S->clip_depth]      = ( uint8_t ) FLUX_XTK_CLIP_ELLIPSE;
	S->clip_geometry [S->clip_depth]  = ( ID2D1Geometry * ) geo;
	S->clip_depth                    += 1u;
	return true;
}

static bool xtk_push_clip_rounded(FluxBackendXtkState *S, XentRect rect, float radius) {
	if (S->clip_depth >= FLUX_XTK_CLIP_CAP) return false;
	ID2D1Factory *factory = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(S->rc), &factory);
	if (!factory) return false;
	D2D1_ROUNDED_RECT rr;
	rr.rect.left                       = rect.x;
	rr.rect.top                        = rect.y;
	rr.rect.right                      = rect.x + rect.w;
	rr.rect.bottom                     = rect.y + rect.h;
	rr.radiusX                         = radius;
	rr.radiusY                         = radius;
	ID2D1RoundedRectangleGeometry *geo = NULL;
	HRESULT                        hr  = ID2D1Factory_CreateRoundedRectangleGeometry(factory, &rr, &geo);
	ID2D1Factory_Release(factory);
	if (FAILED(hr) || !geo) return false;
	D2D1_LAYER_PARAMETERS lp = xtk_layer_parameters();
	lp.geometricMask         = ( ID2D1Geometry * ) geo;
	ID2D1RenderTarget_PushLayer(FLUX_RT(S->rc), &lp, NULL);
	S->clip_kind [S->clip_depth]      = ( uint8_t ) FLUX_XTK_CLIP_ROUNDED;
	S->clip_geometry [S->clip_depth]  = ( ID2D1Geometry * ) geo;
	S->clip_depth                    += 1u;
	return true;
}

static bool xtk_pop_clip(FluxBackendXtkState *S) {
	if (S->clip_depth == 0u) return false;
	S->clip_depth                    -= 1u;
	uint8_t        kind               = S->clip_kind [S->clip_depth];
	ID2D1Geometry *geometry           = S->clip_geometry [S->clip_depth];
	S->clip_geometry [S->clip_depth]  = NULL;
	if (kind == FLUX_XTK_CLIP_RECT) ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(S->rc));
	else {
		ID2D1RenderTarget_PopLayer(FLUX_RT(S->rc));
		if (geometry) ID2D1Geometry_Release(geometry);
	}
	return true;
}

static bool xtk_push_transform(FluxBackendXtkState *S, XentAffine2 const *m) {
	if (S->xform_depth >= 32u) return false;
	ID2D1RenderTarget_GetTransform(FLUX_RT(S->rc), &S->xform_stack [S->xform_depth++]);
	D2D1_MATRIX_3X2_F current;
	ID2D1RenderTarget_GetTransform(FLUX_RT(S->rc), &current);
	D2D1_MATRIX_3X2_F local    = xtk_affine_to_d2d(m);
	D2D1_MATRIX_3X2_F combined = xtk_mul_affine(local, current);
	ID2D1RenderTarget_SetTransform(FLUX_RT(S->rc), &combined);
	return true;
}

static ID2D1StrokeStyle *xtk_stroke_style_for_cap(ID2D1Factory *factory, XentStrokeCap cap) {
	D2D1_STROKE_STYLE_PROPERTIES ssp;
	memset(&ssp, 0, sizeof(ssp));
	D2D1_CAP_STYLE cap_style = D2D1_CAP_STYLE_FLAT;
	switch (cap) {
	case XENT_STROKE_CAP_ROUND  : cap_style = D2D1_CAP_STYLE_ROUND; break;
	case XENT_STROKE_CAP_SQUARE : cap_style = D2D1_CAP_STYLE_SQUARE; break;
	default                     : break;
	}
	ssp.startCap                   = cap_style;
	ssp.endCap                     = cap_style;
	ssp.dashCap                    = cap_style;
	ssp.lineJoin                   = D2D1_LINE_JOIN_ROUND;
	ssp.miterLimit                 = 1.0f;
	ssp.dashStyle                  = D2D1_DASH_STYLE_SOLID;
	ssp.dashOffset                 = 0.0f;
	ID2D1StrokeStyle *stroke_style = NULL;
	ID2D1Factory_CreateStrokeStyle(factory, &ssp, NULL, 0, &stroke_style);
	return stroke_style;
}

static bool xtk_stroke_arc(FluxBackendXtkState *S, XentDlCmd const *cmd) {
	if (cmd->u.arc.stroke_width <= 0.0f || cmd->u.arc.sweep_deg == 0.0f) return true;
	XentRect bounds = cmd->u.arc.bounds;
	float    cx = bounds.x + bounds.w * 0.5f, cy = bounds.y + bounds.h * 0.5f;
	float    rx = bounds.w * 0.5f, ry = bounds.h * 0.5f;
	if (rx <= 0.0f || ry <= 0.0f) return true;
	float         start   = cmd->u.arc.start_deg * (3.14159265f / 180.0f);
	float         sweep   = cmd->u.arc.sweep_deg * (3.14159265f / 180.0f);
	float         end     = start + sweep;
	D2D1_POINT_2F p0      = {cx + rx * cosf(start), cy + ry * sinf(start)};
	D2D1_POINT_2F p1      = {cx + rx * cosf(end), cy + ry * sinf(end)};

	ID2D1Factory *factory = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(S->rc), &factory);
	if (!factory) return false;
	ID2D1PathGeometry *path = NULL;
	if (FAILED(ID2D1Factory_CreatePathGeometry(factory, &path)) || !path) {
		ID2D1Factory_Release(factory);
		return false;
	}
	ID2D1GeometrySink *sink = NULL;
	if (FAILED(ID2D1PathGeometry_Open(path, &sink)) || !sink) {
		ID2D1PathGeometry_Release(path);
		ID2D1Factory_Release(factory);
		return false;
	}
	float                abs_sweep = sweep < 0.0f ? -sweep : sweep;
	D2D1_SWEEP_DIRECTION dir = sweep >= 0.0f ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
	D2D1_ARC_SIZE        arc_size = abs_sweep > 3.14159265f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
	D2D1_ARC_SEGMENT     arc      = {
	  p1, {rx, ry},
       0.0f, dir, arc_size
    };
	ID2D1GeometrySink_BeginFigure(sink, p0, D2D1_FIGURE_BEGIN_HOLLOW);
	ID2D1GeometrySink_AddArc(sink, &arc);
	ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_OPEN);
	ID2D1GeometrySink_Close(sink);
	ID2D1GeometrySink_Release(sink);
	flux_set_brush(S->rc, xtk_color_to_flux(cmd->u.arc.color));
	ID2D1StrokeStyle *stroke_style = xtk_stroke_style_for_cap(factory, cmd->u.arc.cap);
	ID2D1RenderTarget_DrawGeometry(
	  FLUX_RT(S->rc), ( ID2D1Geometry * ) path, ( ID2D1Brush * ) S->rc->brush, cmd->u.arc.stroke_width, stroke_style
	);
	if (stroke_style) ID2D1StrokeStyle_Release(stroke_style);
	ID2D1PathGeometry_Release(path);
	ID2D1Factory_Release(factory);
	return true;
}

static ID2D1PathGeometry *xtk_tab_selected_path(FluxBackendXtkState *S, XentRect bounds) {
	if (bounds.w < 2.0f * FLUX_TAB_SELECTED_CORNER || bounds.h < FLUX_TAB_SELECTED_CORNER + FLUX_TAB_SELECTED_FLARE)
		return NULL;
	ID2D1Factory *factory = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(S->rc), &factory);
	if (!factory) return NULL;
	ID2D1PathGeometry *geo = NULL;
	if (FAILED(ID2D1Factory_CreatePathGeometry(factory, &geo)) || !geo) {
		ID2D1Factory_Release(factory);
		return NULL;
	}
	ID2D1GeometrySink *sink = NULL;
	if (FAILED(ID2D1PathGeometry_Open(geo, &sink)) || !sink) {
		ID2D1PathGeometry_Release(geo);
		ID2D1Factory_Release(factory);
		return NULL;
	}
	float            f = FLUX_TAB_SELECTED_FLARE, c = FLUX_TAB_SELECTED_CORNER;
	float            x0 = bounds.x, x1 = bounds.x + bounds.w, y0 = bounds.y, y1 = bounds.y + bounds.h;
	D2D1_ARC_SEGMENT arc = {
	  {x0, y1 - f},
      {f,  f     },
      0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL
    };
	ID2D1GeometrySink_SetFillMode(sink, D2D1_FILL_MODE_WINDING);
	ID2D1GeometrySink_BeginFigure(sink, flux_point(x0 - f, y1), D2D1_FIGURE_BEGIN_FILLED);
	ID2D1GeometrySink_AddArc(sink, &arc);
	ID2D1GeometrySink_AddLine(sink, flux_point(x0, y0 + c));
	arc = (D2D1_ARC_SEGMENT) {
	  {x0 + c, y0},
      {c,      c },
      0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL
    };
	ID2D1GeometrySink_AddArc(sink, &arc);
	ID2D1GeometrySink_AddLine(sink, flux_point(x1 - c, y0));
	arc = (D2D1_ARC_SEGMENT) {
	  {x1, y0 + c},
      {c,  c     },
      0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL
    };
	ID2D1GeometrySink_AddArc(sink, &arc);
	ID2D1GeometrySink_AddLine(sink, flux_point(x1, y1 - f));
	arc = (D2D1_ARC_SEGMENT) {
	  {x1 + f, y1},
      {f,      f },
      0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL
    };
	ID2D1GeometrySink_AddArc(sink, &arc);
	ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
	ID2D1GeometrySink_Close(sink);
	ID2D1GeometrySink_Release(sink);
	ID2D1Factory_Release(factory);
	return geo;
}

static bool xtk_fill_shape(FluxBackendXtkState *S, XentDlCmd const *cmd) {
	if (cmd->u.shape.shape != FLUX_SHAPE_TAB_SELECTED_FRAME) return true;
	ID2D1PathGeometry *geo = xtk_tab_selected_path(S, cmd->u.shape.bounds);
	if (!geo) return false;
	flux_set_brush(S->rc, xtk_color_to_flux(cmd->u.shape.color));
	ID2D1RenderTarget_FillGeometry(FLUX_RT(S->rc), ( ID2D1Geometry * ) geo, ( ID2D1Brush * ) S->rc->brush, NULL);
	ID2D1PathGeometry_Release(geo);
	return true;
}

static bool xtk_replay_cmd(FluxBackendXtkState *S, XentDl const *list, XentDlCmd const *cmd) {
	if (!S->rc || !S->rc->d2d || !S->rc->brush) return false;

	switch (cmd->op) {
	case XENT_DL_FILLRECT :
		flux_fill_rect(
		  S->rc, &(FluxRect) {cmd->u.geom.rect.x, cmd->u.geom.rect.y, cmd->u.geom.rect.w, cmd->u.geom.rect.h},
		  xtk_color_to_flux(cmd->u.geom.color)
		);
		return true;
	case XENT_DL_STROKERECT :
		if (cmd->u.geom.stroke_width <= 0.0f) return true;
		flux_set_brush(S->rc, xtk_color_to_flux(cmd->u.geom.color));
		{
			FluxRect    r  = {cmd->u.geom.rect.x, cmd->u.geom.rect.y, cmd->u.geom.rect.w, cmd->u.geom.rect.h};
			D2D1_RECT_F dr = flux_d2d_rect(&r);
			ID2D1RenderTarget_DrawRectangle(
			  FLUX_RT(S->rc), &dr, ( ID2D1Brush * ) S->rc->brush, cmd->u.geom.stroke_width, NULL
			);
		}
		return true;
	case XENT_DL_FILLRRECT :
		flux_fill_rounded_rect(
		  S->rc, &(FluxRect) {cmd->u.geom.rect.x, cmd->u.geom.rect.y, cmd->u.geom.rect.w, cmd->u.geom.rect.h},
		  cmd->u.geom.radius, xtk_color_to_flux(cmd->u.geom.color)
		);
		return true;
	case XENT_DL_STROKERRECT :
		if (cmd->u.geom.stroke_width <= 0.0f) return true;
		flux_draw_rounded_rect(
		  S->rc, &(FluxRect) {cmd->u.geom.rect.x, cmd->u.geom.rect.y, cmd->u.geom.rect.w, cmd->u.geom.rect.h},
		  cmd->u.geom.radius, xtk_color_to_flux(cmd->u.geom.color), cmd->u.geom.stroke_width
		);
		return true;
	case XENT_DL_DRAWTEXT         : xtk_draw_text(S, list, cmd); return true;
	case XENT_DL_DRAWIMG          : xtk_draw_image(S, cmd); return true;
	case XENT_DL_PUSHCLIP_RECT    : return xtk_push_clip_rect(S, cmd->u.geom.rect);
	case XENT_DL_PUSHCLIP_ELLIPSE : return xtk_push_clip_ellipse(S, cmd->u.ellipse.bounds);
	case XENT_DL_PUSHCLIP_RRECT   : return xtk_push_clip_rounded(S, cmd->u.geom.rect, cmd->u.geom.radius);
	case XENT_DL_POPCLIP          : return xtk_pop_clip(S);
	case XENT_DL_PUSHTRANSFORM    : return xtk_push_transform(S, &cmd->u.transform.m);
	case XENT_DL_POPTRANSFORM :
		if (S->xform_depth == 0u) return false;
		ID2D1RenderTarget_SetTransform(FLUX_RT(S->rc), &S->xform_stack [--S->xform_depth]);
		return true;
	case XENT_DL_PUSHOPACITY : {
		D2D1_LAYER_PARAMETERS lp = xtk_layer_parameters();
		lp.opacity               = cmd->u.opacity.opacity;
		ID2D1RenderTarget_PushLayer(FLUX_RT(S->rc), &lp, NULL);
		return true;
	}
	case XENT_DL_POPOPACITY  : ID2D1RenderTarget_PopLayer(FLUX_RT(S->rc)); return true;
	case XENT_DL_FILLELLIPSE : {
		D2D1_ELLIPSE    e    = xtk_ellipse(cmd->u.ellipse.bounds);
		FluxEllipseSpec spec = {e.point.x, e.point.y, e.radiusX, e.radiusY};
		flux_fill_ellipse(S->rc, &spec, xtk_color_to_flux(cmd->u.ellipse.color));
		return true;
	}
	case XENT_DL_STROKEELLIPSE : {
		if (cmd->u.ellipse.stroke_width <= 0.0f) return true;
		D2D1_ELLIPSE    e    = xtk_ellipse(cmd->u.ellipse.bounds);
		FluxEllipseSpec spec = {e.point.x, e.point.y, e.radiusX, e.radiusY};
		flux_stroke_ellipse(S->rc, &spec, xtk_color_to_flux(cmd->u.ellipse.color), cmd->u.ellipse.stroke_width);
		return true;
	}
	case XENT_DL_STROKEARC : return xtk_stroke_arc(S, cmd);
	case XENT_DL_STROKELINE :
		if (cmd->u.line.stroke_width <= 0.0f) return true;
		{
			FluxLineSpec line = {cmd->u.line.x0, cmd->u.line.y0, cmd->u.line.x1, cmd->u.line.y1};
			flux_draw_line(S->rc, &line, xtk_color_to_flux(cmd->u.line.color), cmd->u.line.stroke_width);
		}
		return true;
	case XENT_DL_FILLSHAPE        : return xtk_fill_shape(S, cmd);
	case XENT_DL_FILLTRI          : return xtk_fill_triangle(S, cmd);
	case XENT_DL_STROKERRECT_GRAD : return xtk_stroke_rounded_linear(S, cmd);
	case XENT_DL_FILLRECT_GRAD    : return xtk_fill_rect_linear(S, cmd);
	default                       : return true;
	}
}

static bool xtk_replay_state(FluxBackendXtkState *S, XentDl const *list) {
	if (!S || !S->rc || !S->rc->d2d || !S->rc->brush || !list) return false;
	S->xform_depth = 0u;
	S->clip_depth  = 0u;

	uint32_t count = xent_dl_ncmd(list);
	bool     ok    = true;
	for (uint32_t i = 0; i < count; ++i) {
		XentDlCmd const *cmd = xent_dl_cmd(list, i);
		if (!cmd) continue;
		if (!xtk_replay_cmd(S, list, cmd)) ok = false;
	}
	while (S->clip_depth > 0u) {
		( void ) xtk_pop_clip(S);
		ok = false;
	}
	while (S->xform_depth > 0u) {
		ID2D1RenderTarget_SetTransform(FLUX_RT(S->rc), &S->xform_stack [--S->xform_depth]);
		ok = false;
	}
	return ok;
}

static bool xtk_replay_display_list(XtkBackend *backend, XentDl const *list) {
	return xtk_replay_state(xtk_state(backend), list);
}

bool flux_xtk_replay_rc(FluxRenderCtx const *rc, XentDl const *list) {
	FluxBackendXtkState S;
	memset(&S, 0, sizeof(S));
	S.rc = rc;
	return xtk_replay_state(&S, list);
}

static void const *xtk_query_capability(XtkBackend *backend, XtkCapabilityId id) {
	( void ) backend;
	( void ) id;
	return NULL;
}

static XtkHost *xtk_backend_host(XtkBackend *backend) {
	FluxBackendXtkState *S = xtk_state(backend);
	return S ? S->host : NULL;
}

static XtkBackendVtable const flux_xtk_backend_vtable = {
  .abi_version         = FLUX_XTK_BACKEND_ABI,
  .replay_display_list = xtk_replay_display_list,
  .query_capability    = xtk_query_capability,
  .host                = xtk_backend_host,
};

XtkBackend *flux_xtk_backend_create(FluxBackendCtx *bctx, XtkHost *host) {
	if (!bctx || !host) return NULL;
	FluxBackendXtkState *S = ( FluxBackendXtkState * ) calloc(1, sizeof(*S));
	if (!S) return NULL;
	S->bctx         = *bctx;
	S->host         = host;
	S->backend.vt   = &flux_xtk_backend_vtable;
	S->backend.impl = S;
	return &S->backend;
}

FluxBackendCtx *flux_xtk_backendctx(XtkBackend *backend) {
	FluxBackendXtkState *S = xtk_state(backend);
	return S ? &S->bctx : NULL;
}

void flux_xtk_backend_destroy(XtkBackend *backend) {
	FluxBackendXtkState *S = xtk_state(backend);
	free(S);
}

void flux_xtk_setrc(XtkBackend *backend, FluxRenderCtx const *rc) {
	FluxBackendXtkState *S = xtk_state(backend);
	if (S) S->rc = rc;
}
