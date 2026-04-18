#include "../flux_fluent.h"

void flux_draw_progress_ring(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxThemeColors const *t        = rc->theme;

	/* Center the ring in the provided bounds */
	float                  size     = bounds->w < bounds->h ? bounds->w : bounds->h;
	float                  stroke_w = 4.0f;
	float                  radius   = (size - stroke_w) * 0.5f;
	if (radius < 2.0f) return;

	float         cx           = bounds->x + bounds->w * 0.5f;
	float         cy           = bounds->y + bounds->h * 0.5f;

	FluxColor     track_color  = t ? t->ctrl_strong_fill_default : ft_ctrl_strong_fill_default();
	FluxColor     accent_color = t ? t->accent_default : ft_accent_default();

	/* Get factory from the render target */
	ID2D1Factory *factory      = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(rc), &factory);
	if (!factory) return;

	/* --- Draw the track (full circle) --- */
	flux_stroke_ellipse(rc, cx, cy, radius, radius, track_color, stroke_w);

	/* Determine mode */
	bool  determinate = !snap->indeterminate && snap->max_value > 0.0f;

	float start_rad, sweep_rad;

		if (determinate) {
			/* Determinate: arc from 12 o'clock, clockwise, proportional to value */
			float pct = snap->current_value / snap->max_value;
			if (pct < 0.0f) pct = 0.0f;
			if (pct > 1.0f) pct = 1.0f;
				if (pct < 0.001f) {
					/* Nothing to draw */
					ID2D1Factory_Release(factory);
					return;
				}

			start_rad = 0.0f; /* 12 o'clock */
			sweep_rad = pct * 2.0f * 3.14159265f;
		}
		else {
			/* Indeterminate: continuously rotating arc with oscillating length */
			double now             = rc->now;
			/* One full rotation per second */
			float  rotation        = ( float ) fmod(now * 2.0 * 3.14159265, 2.0 * 3.14159265);
			/* Arc length oscillates between 30 and 270 degrees */
			float  t_osc           = ( float ) (sin(now * 2.5) * 0.5 + 0.5); /* [0, 1] */
			float  min_sweep       = 30.0f * (3.14159265f / 180.0f);
			float  max_sweep       = 270.0f * (3.14159265f / 180.0f);
			sweep_rad              = min_sweep + t_osc * (max_sweep - min_sweep);
			start_rad              = rotation;

			*rc->animations_active = true;
		}

	/* Clamp sweep to just under full circle to avoid degenerate arc */
	float pi2 = 2.0f * 3.14159265f;
	if (sweep_rad > pi2 - 0.001f) sweep_rad = pi2 - 0.001f;

	/* Calculate start and end points on the circle.
	 * Convention: 0 rad = 12 o'clock, clockwise positive.
	 * x = cx + radius * sin(angle)
	 * y = cy - radius * cos(angle)
	 */
	float              end_rad = start_rad + sweep_rad;
	float              start_x = cx + radius * sinf(start_rad);
	float              start_y = cy - radius * cosf(start_rad);
	float              end_x   = cx + radius * sinf(end_rad);
	float              end_y   = cy - radius * cosf(end_rad);

	/* Create path geometry for the arc */
	ID2D1PathGeometry *path    = NULL;
	HRESULT            hr      = ID2D1Factory_CreatePathGeometry(factory, &path);
		if (FAILED(hr) || !path) {
			ID2D1Factory_Release(factory);
			return;
		}

	ID2D1GeometrySink *sink = NULL;
	hr                      = ID2D1PathGeometry_Open(path, &sink);
		if (FAILED(hr) || !sink) {
			ID2D1PathGeometry_Release(path);
			ID2D1Factory_Release(factory);
			return;
		}

	ID2D1GeometrySink_BeginFigure(sink, flux_point(start_x, start_y), D2D1_FIGURE_BEGIN_HOLLOW);

	D2D1_ARC_SEGMENT arc;
	arc.point          = flux_point(end_x, end_y);
	arc.size.width     = radius;
	arc.size.height    = radius;
	arc.rotationAngle  = 0.0f;
	arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
	arc.arcSize        = (sweep_rad > 3.14159265f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;

	ID2D1GeometrySink_AddArc(sink, &arc);
	ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_OPEN);
	ID2D1GeometrySink_Close(sink);

	/* Create stroke style with round caps */
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

	/* Draw the arc */
	flux_set_brush(rc, accent_color);
	ID2D1RenderTarget_DrawGeometry(
	  FLUX_RT(rc), ( ID2D1Geometry * ) path, ( ID2D1Brush * ) rc->brush, stroke_w, stroke_style
	);

	/* Clean up */
	if (stroke_style) ID2D1StrokeStyle_Release(stroke_style);
	ID2D1GeometrySink_Release(sink);
	ID2D1PathGeometry_Release(path);
	ID2D1Factory_Release(factory);
}
