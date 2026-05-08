#include "render/flux_fluent.h"
#include <math.h>
#include <string.h>

#define PROGRESS_RING_PI            3.14159265f
#define PROGRESS_RING_TWO_PI        (2.0f * PROGRESS_RING_PI)
#define PROGRESS_RING_DEG_TO_RAD    (PROGRESS_RING_PI / 180.0f)
#define PROGRESS_RING_MIN_SWEEP_DEG 30.0f
#define PROGRESS_RING_MAX_SWEEP_DEG 270.0f
#define PROGRESS_RING_ROTATION_HZ   1.0
#define PROGRESS_RING_OSC_HZ        2.5
#define PROGRESS_RING_STROKE_W      4.0f
#define PROGRESS_RING_MIN_RADIUS    2.0f

typedef struct ProgressRingArc {
	float cx;
	float cy;
	float radius;
	float start_rad;
	float sweep_rad;
} ProgressRingArc;

static bool
progress_ring_angles(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, float *start_rad, float *sweep_rad) {
	if (!snap->indeterminate && snap->max_value > 0.0f) {
		float pct = snap->current_value / snap->max_value;
		if (pct < 0.0f) pct = 0.0f;
		if (pct > 1.0f) pct = 1.0f;
		if (pct < FLUX_ANIM_VALUE_EPS) return false;

		*start_rad = 0.0f;
		*sweep_rad = pct * PROGRESS_RING_TWO_PI;
		return true;
	}

	double now       = rc->now;
	float  rotation  = ( float ) fmod(now * PROGRESS_RING_ROTATION_HZ * PROGRESS_RING_TWO_PI, PROGRESS_RING_TWO_PI);
	float  t_osc     = ( float ) (sin(now * PROGRESS_RING_OSC_HZ) * 0.5 + 0.5);
	float  min_sweep = PROGRESS_RING_MIN_SWEEP_DEG * PROGRESS_RING_DEG_TO_RAD;
	float  max_sweep = PROGRESS_RING_MAX_SWEEP_DEG * PROGRESS_RING_DEG_TO_RAD;

	*start_rad       = rotation;
	*sweep_rad       = min_sweep + t_osc * (max_sweep - min_sweep);
	if (rc->animations_active) *rc->animations_active = true;
	return true;
}

static ID2D1PathGeometry *progress_ring_create_path(ID2D1Factory *factory, ProgressRingArc const *arc_spec) {
	float              end_rad = arc_spec->start_rad + arc_spec->sweep_rad;
	float              start_x = arc_spec->cx + arc_spec->radius * sinf(arc_spec->start_rad);
	float              start_y = arc_spec->cy - arc_spec->radius * cosf(arc_spec->start_rad);
	float              end_x   = arc_spec->cx + arc_spec->radius * sinf(end_rad);
	float              end_y   = arc_spec->cy - arc_spec->radius * cosf(end_rad);

	ID2D1PathGeometry *path    = NULL;
	if (FAILED(ID2D1Factory_CreatePathGeometry(factory, &path)) || !path) return NULL;

	ID2D1GeometrySink *sink = NULL;
	if (FAILED(ID2D1PathGeometry_Open(path, &sink)) || !sink) {
		ID2D1PathGeometry_Release(path);
		return NULL;
	}

	D2D1_ARC_SEGMENT arc;
	arc.point          = flux_point(end_x, end_y);
	arc.size.width     = arc_spec->radius;
	arc.size.height    = arc_spec->radius;
	arc.rotationAngle  = 0.0f;
	arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
	arc.arcSize        = arc_spec->sweep_rad > PROGRESS_RING_PI ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;

	ID2D1GeometrySink_BeginFigure(sink, flux_point(start_x, start_y), D2D1_FIGURE_BEGIN_HOLLOW);
	ID2D1GeometrySink_AddArc(sink, &arc);
	ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_OPEN);
	ID2D1GeometrySink_Close(sink);
	ID2D1GeometrySink_Release(sink);
	return path;
}

static ID2D1StrokeStyle *progress_ring_create_stroke_style(ID2D1Factory *factory) {
	ID2D1StrokeStyle            *stroke_style = NULL;
	D2D1_STROKE_STYLE_PROPERTIES ssp;
	memset(&ssp, 0, sizeof(ssp));
	ssp.startCap   = D2D1_CAP_STYLE_ROUND;
	ssp.endCap     = D2D1_CAP_STYLE_ROUND;
	ssp.dashCap    = D2D1_CAP_STYLE_ROUND;
	ssp.lineJoin   = D2D1_LINE_JOIN_ROUND;
	ssp.miterLimit = 1.0f;
	ssp.dashStyle  = D2D1_DASH_STYLE_SOLID;
	ssp.dashOffset = 0.0f;
	ID2D1Factory_CreateStrokeStyle(factory, &ssp, NULL, 0, &stroke_style);
	return stroke_style;
}

void flux_draw_progress_ring(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;

	float size     = bounds->w < bounds->h ? bounds->w : bounds->h;
	float stroke_w = PROGRESS_RING_STROKE_W;
	float radius   = (size - stroke_w) * 0.5f;
	if (radius < PROGRESS_RING_MIN_RADIUS) return;

	float                  cx           = bounds->x + bounds->w * 0.5f;
	float                  cy           = bounds->y + bounds->h * 0.5f;
	FluxThemeColors const *t            = rc->theme;
	FluxColor              track_color  = t ? t->ctrl_strong_fill_default : ft_ctrl_strong_fill_default();
	FluxColor              accent_color = t ? t->accent_default : ft_accent_default();

	ID2D1Factory          *factory      = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(rc), &factory);
	if (!factory) return;

	flux_stroke_ellipse(rc, &(FluxEllipseSpec) {cx, cy, radius, radius}, track_color, stroke_w);

	float start_rad = 0.0f;
	float sweep_rad = 0.0f;
	if (!progress_ring_angles(rc, snap, &start_rad, &sweep_rad)) {
		ID2D1Factory_Release(factory);
		return;
	}

	if (sweep_rad > PROGRESS_RING_TWO_PI - FLUX_ANIM_VALUE_EPS) sweep_rad = PROGRESS_RING_TWO_PI - FLUX_ANIM_VALUE_EPS;

	ProgressRingArc    arc  = {cx, cy, radius, start_rad, sweep_rad};
	ID2D1PathGeometry *path = progress_ring_create_path(factory, &arc);
	if (!path) {
		ID2D1Factory_Release(factory);
		return;
	}

	ID2D1StrokeStyle *stroke_style = progress_ring_create_stroke_style(factory);
	flux_set_brush(rc, accent_color);
	ID2D1RenderTarget_DrawGeometry(
	  FLUX_RT(rc), ( ID2D1Geometry * ) path, ( ID2D1Brush * ) rc->brush, stroke_w, stroke_style
	);

	if (stroke_style) ID2D1StrokeStyle_Release(stroke_style);
	ID2D1PathGeometry_Release(path);
	ID2D1Factory_Release(factory);
}
