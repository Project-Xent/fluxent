#include "render/flux_fluent.h"

typedef struct {
	float toggle;
	float hover;
	float press;
} SwitchAnim;

typedef struct {
	FluxColor off_normal;
	FluxColor off_hover;
	FluxColor off_press;
	FluxColor on_normal;
	FluxColor on_hover;
	FluxColor on_press;
} SwitchTrackColors;

static SwitchAnim switch_update_anim(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxControlState const *state, bool on
) {
	FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	if (!ce) return (SwitchAnim) {on ? 1.0f : 0.0f, state->hovered ? 1.0f : 0.0f, state->pressed ? 1.0f : 0.0f};

	FluxTweenChannels channels  = {&ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim};
	FluxTweenTargets  targets   = {state->hovered, state->pressed, state->focused, on};
	bool              animating = flux_tween_update_states(&channels, targets, rc->now);
	if (animating && rc->animations_active) *rc->animations_active = true;
	return (SwitchAnim) {ce->check_anim.current, ce->hover_anim.current, ce->press_anim.current};
}

static SwitchTrackColors switch_track_colors(FluxControlState const *state, FluxThemeColors const *t) {
	SwitchTrackColors c = {0};
	if (!state->enabled) {
		c.off_normal = c.off_hover = c.off_press = t ? t->ctrl_alt_fill_disabled : flux_color_rgba(0, 0, 0, 0);
		c.on_normal = c.on_hover = c.on_press = t ? t->accent_disabled : flux_color_rgba(0, 0, 0, 0x37);
		return c;
	}

	c.off_normal = t ? t->ctrl_alt_fill_secondary : flux_color_rgba(0, 0, 0, 0x06);
	c.off_hover  = t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 0x0f);
	c.off_press  = t ? t->ctrl_alt_fill_quarternary : flux_color_rgba(0, 0, 0, 0x12);
	c.on_normal  = t ? t->accent_default : flux_color_rgb(0, 120, 212);
	c.on_hover   = t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xe6);
	c.on_press   = t ? t->accent_tertiary : flux_color_rgba(0, 120, 212, 0xcc);
	return c;
}

static FluxColor switch_track_fill(SwitchTrackColors colors, SwitchAnim anim) {
	FluxColor off_color = flux_anim_lerp_color(
	  flux_anim_lerp_color(colors.off_normal, colors.off_hover, anim.hover), colors.off_press, anim.press
	);
	FluxColor on_color = flux_anim_lerp_color(
	  flux_anim_lerp_color(colors.on_normal, colors.on_hover, anim.hover), colors.on_press, anim.press
	);
	return flux_anim_lerp_color(off_color, on_color, anim.toggle);
}

static void draw_switch_off_stroke(
  FluxRenderContext const *rc, FluxRect const *track, FluxControlState const *state, SwitchAnim anim, float track_r
) {
	if (anim.toggle >= 0.5f) return;

	FluxThemeColors const *t = rc->theme;
	FluxColor off_stroke     = !state->enabled ? (t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37))
	                                           : (t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 0x9c));
	if (anim.toggle > 0.0f) {
		float   fade    = 1.0f - anim.toggle * 2.0f;
		uint8_t sa      = ( uint8_t ) (flux_color_af(off_stroke) * fade * 255.0f);
		off_stroke.rgba = (off_stroke.rgba & 0xffffff00) | sa;
	}
	flux_draw_rounded_rect(rc, track, track_r, off_stroke, 1.0f);
}

static FluxColor switch_thumb_fill(FluxControlState const *state, FluxThemeColors const *t, float toggle_t) {
	FluxColor knob_off;
	FluxColor knob_on;
	if (!state->enabled) {
		knob_off = t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5c);
		knob_on  = t ? t->text_on_accent_disabled : flux_color_rgb(255, 255, 255);
	}
	else {
		knob_off = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9e);
		knob_on  = t ? t->text_on_accent_primary : flux_color_rgb(255, 255, 255);
	}
	return flux_anim_lerp_color(knob_off, knob_on, toggle_t);
}

static FluxRect switch_thumb_rect(FluxRect const *track, FluxControlState const *state, SwitchAnim anim, float cy) {
	float off_x   = track->x + track->h * 0.5f;
	float on_x    = track->x + track->w - track->h * 0.5f;
	float thumb_x = flux_anim_mixf(off_x, on_x, anim.toggle);
	if (state->enabled) thumb_x += (1.0f - 2.0f * anim.toggle) * FLUX_TOGGLE_TRAVEL_EXT * anim.press;

	float knob_w = 12.0f;
	float knob_h = 12.0f;
	if (state->enabled) {
		knob_w = 12.0f + (14.0f - 12.0f) * anim.hover + (17.0f - 14.0f) * anim.press;
		knob_h = 12.0f + (14.0f - 12.0f) * anim.hover;
	}

	return (FluxRect) {thumb_x - knob_w * 0.5f, cy - knob_h * 0.5f, knob_w, knob_h};
}

void flux_draw_switch(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t          = rc->theme;

	float                  track_w    = 40.0f;
	float                  track_h    = 20.0f;
	float                  track_r    = track_h * 0.5f;

	float                  cy         = bounds->y + bounds->h * 0.5f;
	FluxRect               track      = {bounds->x, cy - track_h * 0.5f, track_w, track_h};

	bool                   on         = (snap->check_state == FLUX_CHECK_CHECKED);

	SwitchAnim             anim       = switch_update_anim(rc, snap, state, on);
	FluxColor              track_fill = switch_track_fill(switch_track_colors(state, t), anim);
	flux_fill_rounded_rect(rc, &track, track_r, track_fill);
	draw_switch_off_stroke(rc, &track, state, anim, track_r);

	FluxColor thumb_fill  = switch_thumb_fill(state, t, anim.toggle);
	FluxRect  knob_rect   = switch_thumb_rect(&track, state, anim, cy);
	float     knob_radius = flux_minf(knob_rect.w, knob_rect.h) * 0.5f;
	flux_fill_rounded_rect(rc, &knob_rect, knob_radius, thumb_fill);

	if (state->focused && state->enabled) flux_draw_focus_rect(rc, &track, track_r);
}
