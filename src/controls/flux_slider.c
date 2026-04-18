#include "../flux_render_internal.h"
#include <math.h>

void flux_draw_slider(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t           = rc->theme;

	float                  track_h     = 4.0f;
	float                  thumb_outer = 10.0f;

	float                  range       = snap->max_value - snap->min_value;
	float                  pct         = (range > 0.0f) ? (snap->current_value - snap->min_value) / range : 0.0f;
	if (pct < 0.0f) pct = 0.0f;
	if (pct > 1.0f) pct = 1.0f;

	float cy         = bounds->y + bounds->h * 0.5f;
	float pad        = thumb_outer;
	float track_left = bounds->x + pad;
	float track_w    = bounds->w - pad * 2.0f;
	if (track_w < 0.0f) track_w = 0.0f;

	/* ── Hover/press tweens for smooth color transitions ── */
	FluxCacheEntry *ce      = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	float           hover_t = state->hovered ? 1.0f : 0.0f;
	float           press_t = state->pressed ? 1.0f : 0.0f;
		if (ce) {
			bool animating = flux_tween_update_states(
			  &ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim, state->hovered, state->pressed,
			  state->focused, false, rc->now
			);
			if (animating && rc->animations_active) *rc->animations_active = true;
			hover_t = ce->hover_anim.current;
			press_t = ce->press_anim.current;
		}

	/* Track background — uses StrongFill, not AltFill */
	FluxColor track_bg = t ? t->ctrl_strong_fill_default : flux_color_rgba(0, 0, 0, 0x9c);

	/* Determine value fill, inner thumb color based on interaction state
	   — lerp accent_default → accent_secondary (hover) → accent_tertiary (press). */
	FluxColor val_color, inner_c;
		if (!state->enabled) {
			track_bg  = t ? t->ctrl_strong_fill_disabled : flux_color_rgba(0, 0, 0, 0x37);
			val_color = t ? t->accent_disabled : flux_color_rgba(0, 0, 0, 0x37);
			inner_c   = t ? t->accent_disabled : flux_color_rgba(0, 120, 212, 100);
		}
		else {
			FluxColor acc_n = t ? t->accent_default : flux_color_rgb(0, 120, 212);
			FluxColor acc_h = t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xe6);
			FluxColor acc_p = t ? t->accent_tertiary : flux_color_rgba(0, 120, 212, 0xcc);
			val_color       = flux_anim_lerp_color(flux_anim_lerp_color(acc_n, acc_h, hover_t), acc_p, press_t);
			inner_c         = val_color;
		}

	/* Draw track background */
	FluxRect track = {track_left, cy - track_h * 0.5f, track_w, track_h};
	flux_fill_rounded_rect(rc, &track, track_h * 0.5f, track_bg);

		/* Draw tick marks at each step position along the track */
		if (snap->step > 0.0f && range > 0.0f) {
			int num_steps = ( int ) (range / snap->step);
				if (num_steps > 0 && num_steps <= 100) {
					/* Tick color: same hue as track_bg but slightly more opaque */
					uint8_t   old_alpha  = ( uint8_t ) (track_bg.rgba & 0xff);
					uint8_t   new_alpha  = old_alpha < 0xd0 ? ( uint8_t ) (old_alpha + 0x30) : 0xff;
					FluxColor tick_color = flux_color_rgba(
					  ( uint8_t ) ((track_bg.rgba >> 24) & 0xff), ( uint8_t ) ((track_bg.rgba >> 16) & 0xff),
					  ( uint8_t ) ((track_bg.rgba >> 8) & 0xff), new_alpha
					);

					float tick_half_h = 4.0f;
					float thumb_cur_x = track_left + track_w * pct;

						for (int i = 0; i <= num_steps; i++) {
							float tick_pct = ( float ) i / ( float ) num_steps;
							float tick_x   = track_left + track_w * tick_pct;

							/* Skip tick if it overlaps the current thumb position */
							if (fabsf(tick_x - thumb_cur_x) < 2.0f) continue;

							flux_draw_line(rc, tick_x, cy - tick_half_h, tick_x, cy + tick_half_h, tick_color, 1.0f);
						}
				}
		}

	/* Draw value fill */
	float val_w = track_w * pct;
		if (val_w > 0.0f) {
			FluxRect val_r = {track_left, cy - track_h * 0.5f, val_w, track_h};
			flux_fill_rounded_rect(rc, &val_r, track_h * 0.5f, val_color);
		}

	/* Thumb outer circle */
	float     thumb_x      = track_left + track_w * pct;
	FluxColor thumb_fill   = t ? t->ctrl_solid_fill_default : flux_color_rgb(255, 255, 255);
	FluxColor thumb_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0f);
	if (!state->enabled) thumb_stroke = t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37);
	flux_fill_ellipse(rc, thumb_x, cy, thumb_outer, thumb_outer, thumb_fill);
	flux_stroke_ellipse(rc, thumb_x, cy, thumb_outer, thumb_outer, thumb_stroke, 1.0f);

	/* Inner thumb — scale-based radius (kSliderInnerThumbSize=12, radius=6*scale).
	 * WinUI uses a distinct "Touch" visual state where the thumb grows
	 * (instead of shrinking on press) so finger tracking stays visible.
	 * Mouse: 0.86 rest / 1.00 hover / 0.71 press
	 * Touch: 1.00 rest / 1.00 hover / 1.00 press (thumb doesn't shrink). */
	float target_scale;
	if (state->pointer_type == 1 /* FLUX_POINTER_TOUCH */) target_scale = 1.0f;
	else target_scale = state->pressed ? 0.71f : (state->hovered ? 1.0f : 0.86f);
	float current_scale = target_scale;
		if (ce) {
			float scale;
			bool  animating
			  = flux_tween_update(&ce->slider_anim, target_scale, FLUX_ANIM_DURATION_SLIDER, rc->now, &scale);
			if (animating && rc->animations_active) *rc->animations_active = true;
			current_scale = scale;
		}
	float ir = 6.0f * current_scale;
	flux_fill_ellipse(rc, thumb_x, cy, ir, ir, inner_c);
}
