#include "render/flux_render_internal.h"
#include <math.h>

static FluxColor progress_accent(FluxThemeColors const *t) {
	return t ? t->accent_default : flux_color_rgb(0, 120, 212);
}

static void draw_progress_indeterminate(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, float cy, float track_h,
  float radius
) {
	( void ) snap;
	double const cycle_duration = 2.0;
	double       phase          = fmod(rc->now, cycle_duration);
	float        linear_t       = ( float ) (phase / cycle_duration);
	float        ping_pong      = linear_t < 0.5f ? linear_t * 2.0f : 2.0f - linear_t * 2.0f;
	float        eased          = flux_ease_in_out_cubic(ping_pong);
	float        bar_w          = bounds->w * 0.4f;
	float        travel         = bounds->w - bar_w;
	float        bar_x          = bounds->x + travel * eased;
	FluxRect     fill           = {bar_x, cy - track_h * 0.5f, bar_w, track_h};
	flux_fill_rounded_rect(rc, &fill, radius, progress_accent(rc->theme));
	*rc->animations_active = true;
}

static void draw_progress_determinate(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, float cy, float track_h,
  float radius
) {
	float max_val = snap->max_value > 0.0f ? snap->max_value : 1.0f;
	float pct     = snap->current_value / max_val;
	if (pct < 0.0f) pct = 0.0f;
	if (pct > 1.0f) pct = 1.0f;

	float fill_w = bounds->w * pct;
	if (fill_w <= 0.0f) return;

	FluxRect fill = {bounds->x, cy - track_h * 0.5f, fill_w, track_h};
	flux_fill_rounded_rect(rc, &fill, radius, progress_accent(rc->theme));
}

void flux_draw_progress(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxThemeColors const *t           = rc->theme;

	float                  track_h     = 4.0f;
	float                  radius      = track_h * 0.5f;
	float                  cy          = bounds->y + bounds->h * 0.5f;

	FluxRect               track       = {bounds->x, cy - track_h * 0.5f, bounds->w, track_h};
	FluxColor              track_color = t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 30);
	flux_fill_rounded_rect(rc, &track, radius, track_color);

	if (snap->indeterminate) {
		draw_progress_indeterminate(rc, snap, bounds, cy, track_h, radius);
		return;
	}

	draw_progress_determinate(rc, snap, bounds, cy, track_h, radius);
}
