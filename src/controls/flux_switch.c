#include "../flux_fluent.h"

void flux_draw_switch(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t       = rc->theme;

	float                  track_w = 40.0f;
	float                  track_h = 20.0f;
	float                  track_r = track_h * 0.5f;

	float                  cy      = bounds->y + bounds->h * 0.5f;
	FluxRect               track   = {bounds->x, cy - track_h * 0.5f, track_w, track_h};

	bool                   on      = (snap->check_state == FLUX_CHECK_CHECKED);

	FluxCacheEntry        *ce      = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
		if (ce) {
			bool animating = flux_tween_update_states(
			  &ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim, state->hovered, state->pressed,
			  state->focused, on, rc->now
			);
			if (animating && rc->animations_active) *rc->animations_active = true;
		}

	float     toggle_t = ce ? ce->check_anim.current : (on ? 1.0f : 0.0f);
	float     hover_t  = ce ? ce->hover_anim.current : (state->hovered ? 1.0f : 0.0f);
	float     press_t  = ce ? ce->press_anim.current : (state->pressed ? 1.0f : 0.0f);

	/* ── Track fill — interpolate through normal → hover → press ────── */

	FluxColor off_normal, off_hover, off_press;
	FluxColor on_normal, on_hover, on_press;

		if (!state->enabled) {
			off_normal = off_hover = off_press = t ? t->ctrl_alt_fill_disabled : flux_color_rgba(0, 0, 0, 0);
			on_normal = on_hover = on_press = t ? t->accent_disabled : flux_color_rgba(0, 0, 0, 0x37);
		}
		else {
			off_normal = t ? t->ctrl_alt_fill_secondary : flux_color_rgba(0, 0, 0, 0x06);
			off_hover  = t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 0x0f);
			off_press  = t ? t->ctrl_alt_fill_quarternary : flux_color_rgba(0, 0, 0, 0x12);
			on_normal  = t ? t->accent_default : flux_color_rgb(0, 120, 212);
			on_hover   = t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xe6);
			on_press   = t ? t->accent_tertiary : flux_color_rgba(0, 120, 212, 0xcc);
		}

	/* Blend normal→hover, then that result→press */
	FluxColor off_color
	  = flux_anim_lerp_color(flux_anim_lerp_color(off_normal, off_hover, hover_t), off_press, press_t);
	FluxColor on_color   = flux_anim_lerp_color(flux_anim_lerp_color(on_normal, on_hover, hover_t), on_press, press_t);

	FluxColor track_fill = flux_anim_lerp_color(off_color, on_color, toggle_t);
	flux_fill_rounded_rect(rc, &track, track_r, track_fill);

	/* ── Off-state stroke (fades out as toggle_t → 0.5) ─────────────── */

		if (toggle_t < 0.5f) {
			FluxColor off_stroke = !state->enabled
			                       ? (t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37))
			                       : (t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 0x9c));
				if (toggle_t > 0.0f) {
					float   fade    = 1.0f - toggle_t * 2.0f;
					uint8_t sa      = ( uint8_t ) (flux_color_af(off_stroke) * fade * 255.0f);
					off_stroke.rgba = (off_stroke.rgba & 0xffffff00) | sa;
				}
			flux_draw_rounded_rect(rc, &track, track_r, off_stroke, 1.0f);
		}

	/* ── Thumb position ──────────────────────────────────────────────── */

	float off_x   = track.x + track_r;
	float on_x    = track.x + track_w - track_r;
	float thumb_x = flux_anim_mixf(off_x, on_x, toggle_t);

	/* Travel extension: animate with press_t instead of snapping */
	if (state->enabled) thumb_x += (1.0f - 2.0f * toggle_t) * FLUX_TOGGLE_TRAVEL_EXT * press_t;

	/* ── Knob color ──────────────────────────────────────────────────── */

	FluxColor knob_off, knob_on;
		if (!state->enabled) {
			knob_off = t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5c);
			knob_on  = t ? t->text_on_accent_disabled : flux_color_rgb(255, 255, 255);
		}
		else {
			knob_off = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9e);
			knob_on  = t ? t->text_on_accent_primary : flux_color_rgb(255, 255, 255);
		}
	FluxColor thumb_fill = flux_anim_lerp_color(knob_off, knob_on, toggle_t);

	/* ── Knob size — interpolate through normal → hover → press ─────── */
	/*
	 * Normal 12×12, Hover 14×14, Press 17×14.
	 * Use hover_t and press_t so size changes animate with the same
	 * easing as position, instead of snapping on state transitions.
	 */
	float     knob_w, knob_h;
		if (!state->enabled) {
			knob_w = 12.0f;
			knob_h = 12.0f;
		}
		else {
			knob_w = 12.0f + (14.0f - 12.0f) * hover_t + (17.0f - 14.0f) * press_t;
			knob_h = 12.0f + (14.0f - 12.0f) * hover_t + (14.0f - 14.0f) * press_t;
		}

	float    knob_radius = flux_minf(knob_w, knob_h) * 0.5f;
	FluxRect knob_rect   = {thumb_x - knob_w * 0.5f, cy - knob_h * 0.5f, knob_w, knob_h};
	flux_fill_rounded_rect(rc, &knob_rect, knob_radius, thumb_fill);

	/* ── Focus rect ──────────────────────────────────────────────────── */

	if (state->focused && state->enabled) flux_draw_focus_rect(rc, &track, track_r);
}
