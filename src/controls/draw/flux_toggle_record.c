/**
 * @file flux_toggle_record.c
 * @brief Check-state control recorders (CheckBox / ToggleSwitch / RadioButton):
 * each tweens a shared check animation and may carry a trailing text label.
 */
#include "controls/draw/flux_control_record_internal.h"

#include "render/flux_anim.h"

static bool rec_check_label(
  XentDlBuilder *b, char const *label, FluxRect const *bounds, FluxControlState const *state, FluxThemeColors const *t
) {
	if (!label || !label [0]) return true;
	FluxColor color = state->enabled ? t->text_primary : t->text_disabled;
	FluxRect  tr    = {
	  bounds->x + FLUX_CHECKBOX_SIZE + FLUX_CHECKBOX_GAP, bounds->y,
	  flux_maxf(0.0f, bounds->w - FLUX_CHECKBOX_SIZE - FLUX_CHECKBOX_GAP), bounds->h};
	XentTextStyle style = {.font_size = 14.0f, .font_weight = 400u, .h_align = 0u, .v_align = 1u};
	return flux_rec_text_ex(b, &tr, label, &style, color);
}

typedef struct CheckboxPalette {
	FluxColor unchecked_fill;
	FluxColor checked_fill;
	FluxColor unchecked_stroke;
	FluxColor glyph;
} CheckboxPalette;

static CheckboxPalette checkbox_palette(FluxThemeColors const *t, FluxControlState const *state) {
	CheckboxPalette p
	  = {t->ctrl_fill_default, t->accent_default, t->ctrl_strong_stroke_default, t->text_on_accent_primary};
	if (!state->enabled)
		return (CheckboxPalette) {
		  t->ctrl_fill_disabled, t->accent_disabled, t->ctrl_strong_stroke_disabled, t->text_on_accent_disabled};
	if (state->pressed) {
		p.unchecked_fill = t->ctrl_fill_tertiary;
		p.checked_fill   = t->accent_tertiary;
	}
	else if (state->hovered) {
		p.unchecked_fill = t->ctrl_fill_secondary;
		p.checked_fill   = t->accent_secondary;
	}
	return p;
}

bool flux_rec_build_checkbox(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t   = flux_rec_theme(rc);
	float                  cy  = bounds->y + bounds->h * 0.5f;
	FluxRect               box = {bounds->x, cy - FLUX_CHECKBOX_SIZE * 0.5f, FLUX_CHECKBOX_SIZE, FLUX_CHECKBOX_SIZE};
	bool                   checked = snap->u.check.check_state == FLUX_CHECK_CHECKED;
	FluxRecStateAnim       anim    = flux_rec_animstate(rc, snap, state, checked);

	CheckboxPalette        palette = checkbox_palette(t, state);

	FluxColor              fill    = flux_anim_lerp_color(palette.unchecked_fill, palette.checked_fill, anim.check);
	float                  stroke_alpha = flux_color_af(palette.unchecked_stroke) * (1.0f - anim.check);
	FluxColor              stroke       = flux_color_rgba(
	  (palette.unchecked_stroke.rgba >> 24) & 0xff, (palette.unchecked_stroke.rgba >> 16) & 0xff,
	  (palette.unchecked_stroke.rgba >> 8) & 0xff, ( uint8_t ) (stroke_alpha * 255.0f + 0.5f)
	);

	bool ok = flux_rec_fill_rounded(b, &box, FLUX_CHECKBOX_CORNER, fill);
	if (ok && flux_color_af(stroke) > 0.0f) ok = flux_rec_stroke_rounded(b, &box, FLUX_CHECKBOX_CORNER, stroke, 1.0f);
	if (ok && anim.check > 0.001f) {
		float size = FLUX_CHECKBOX_GLYPH_SIZE * flux_ease_out_quad(anim.check);
		ok         = flux_rec_glyph_wc(b, &box, L"\xE73E", size, palette.glyph);
	}
	if (ok && state->focused && state->enabled) ok = flux_rec_focus_rect(b, &box, FLUX_CHECKBOX_CORNER);
	if (ok) ok = rec_check_label(b, snap->u.check.label, bounds, state, t);
	return ok;
}

bool flux_rec_build_switch(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t     = flux_rec_theme(rc);
	float                  cy    = bounds->y + bounds->h * 0.5f;
	FluxRect               track = {bounds->x, cy - FLUX_TOGGLE_HEIGHT * 0.5f, FLUX_TOGGLE_WIDTH, FLUX_TOGGLE_HEIGHT};
	bool                   on    = snap->u.sw.check_state == FLUX_CHECK_CHECKED;
	FluxRecStateAnim       anim  = flux_rec_animstate(rc, snap, state, on);

	FluxColor              off_n = t->ctrl_alt_fill_secondary, off_h = t->ctrl_alt_fill_tertiary,
						   off_p = t->ctrl_alt_fill_quarternary;
	FluxColor              on_n = t->accent_default, on_h = t->accent_secondary, on_p = t->accent_tertiary;
	if (!state->enabled) {
		off_n = off_h = off_p = t->ctrl_alt_fill_disabled;
		on_n = on_h = on_p = t->accent_disabled;
	}
	FluxColor off_c      = flux_anim_lerp_color(flux_anim_lerp_color(off_n, off_h, anim.hover), off_p, anim.press);
	FluxColor on_c       = flux_anim_lerp_color(flux_anim_lerp_color(on_n, on_h, anim.hover), on_p, anim.press);
	FluxColor track_fill = flux_anim_lerp_color(off_c, on_c, anim.check);

	bool      ok         = flux_rec_fill_rounded(b, &track, FLUX_TOGGLE_HEIGHT * 0.5f, track_fill);
	if (ok && anim.check < 0.5f) {
		FluxColor off_stroke = state->enabled ? t->ctrl_strong_stroke_default : t->ctrl_strong_stroke_disabled;
		if (anim.check > 0.0f) {
			float   fade    = 1.0f - anim.check * 2.0f;
			uint8_t sa      = ( uint8_t ) (flux_color_af(off_stroke) * fade * 255.0f);
			off_stroke.rgba = (off_stroke.rgba & 0xffffff00u) | sa;
		}
		if (flux_color_af(off_stroke) > 0.0f)
			ok = flux_rec_stroke_rounded(b, &track, FLUX_TOGGLE_HEIGHT * 0.5f, off_stroke, 1.0f);
	}

	float off_x   = track.x + track.h * 0.5f;
	float on_x    = track.x + track.w - track.h * 0.5f;
	float thumb_x = flux_anim_mixf(off_x, on_x, anim.check);
	if (state->enabled) thumb_x += (1.0f - 2.0f * anim.check) * FLUX_TOGGLE_TRAVEL_EXT * anim.press;

	float knob_w = FLUX_TOGGLE_KNOB_NORMAL;
	float knob_h = FLUX_TOGGLE_KNOB_NORMAL;
	if (state->enabled) {
		knob_w = FLUX_TOGGLE_KNOB_NORMAL
		       + (FLUX_TOGGLE_KNOB_HOVER - FLUX_TOGGLE_KNOB_NORMAL) * anim.hover
		       + (FLUX_TOGGLE_KNOB_PRESS_W - FLUX_TOGGLE_KNOB_HOVER) * anim.press;
		knob_h = FLUX_TOGGLE_KNOB_NORMAL + (FLUX_TOGGLE_KNOB_HOVER - FLUX_TOGGLE_KNOB_NORMAL) * anim.hover;
	}
	FluxColor knob_off = state->enabled ? t->text_secondary : t->text_disabled;
	FluxColor knob_on  = state->enabled ? t->text_on_accent_primary : t->text_on_accent_disabled;
	FluxColor thumb    = flux_anim_lerp_color(knob_off, knob_on, anim.check);
	FluxRect  knob     = {thumb_x - knob_w * 0.5f, cy - knob_h * 0.5f, knob_w, knob_h};
	float     knob_r   = flux_minf(knob.w, knob.h) * 0.5f;
	if (ok) ok = flux_rec_fill_rounded(b, &knob, knob_r, thumb);
	if (ok && state->focused && state->enabled) ok = flux_rec_focus_rect(b, &track, FLUX_TOGGLE_HEIGHT * 0.5f);
	return ok;
}

bool flux_rec_build_radio(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t       = flux_rec_theme(rc);
	float                  outer   = FLUX_RADIO_SIZE;
	float                  cy      = bounds->y + bounds->h * 0.5f;
	float                  cx      = bounds->x + outer * 0.5f;
	bool                   checked = snap->u.check.check_state == FLUX_CHECK_CHECKED;
	FluxRecStateAnim       anim    = flux_rec_animstate(rc, snap, state, checked);

	FluxColor              outer_fill, outer_stroke, glyph_fill;
	if (!state->enabled) {
		outer_stroke = t->ctrl_strong_stroke_disabled;
		outer_fill   = t->ctrl_alt_fill_disabled;
		glyph_fill   = checked ? t->text_on_accent_disabled : flux_color_rgba(0, 0, 0, 0);
	}
	else if (checked) {
		outer_stroke = flux_anim_lerp_color(
		  flux_anim_lerp_color(t->accent_default, t->accent_secondary, anim.hover), t->accent_tertiary, anim.press
		);
		outer_fill = outer_stroke;
		glyph_fill = t->text_on_accent_primary;
	}
	else {
		outer_stroke = flux_anim_lerp_color(t->ctrl_strong_stroke_default, t->ctrl_strong_stroke_disabled, anim.press);
		outer_fill   = flux_anim_lerp_color(
		  flux_anim_lerp_color(t->ctrl_alt_fill_secondary, t->ctrl_alt_fill_tertiary, anim.hover),
		  t->ctrl_alt_fill_quarternary, anim.press
		);
		glyph_fill = flux_color_rgba(0, 0, 0, 0);
	}

	bool ok = flux_rec_fill_circle(b, cx, cy, outer, outer_fill);
	if (ok) ok = flux_rec_stroke_circle(b, cx, cy, outer, outer_stroke, 1.0f);
	if (ok && anim.check > 0.01f) {
		float target_r = flux_anim_mixf(
		  flux_anim_mixf(FLUX_RADIO_GLYPH, FLUX_RADIO_GLYPH_HOVER, anim.hover), FLUX_RADIO_GLYPH_PRESSED, anim.press
		);
		float inner_d = target_r * 2.0f * flux_ease_out_quad(anim.check);
		ok            = flux_rec_fill_circle(b, cx, cy, inner_d, glyph_fill);
	}
	if (ok && anim.press > 0.01f && !checked && state->enabled) {
		float r = (FLUX_RADIO_PRESS_GLYPH * 0.5f) * anim.press;
		ok      = flux_rec_fill_circle(b, cx, cy, r * 2.0f, t->text_on_accent_primary);
	}
	if (ok && state->focused && state->enabled) {
		FluxRect focus = {bounds->x, cy - outer * 0.5f, outer, outer};
		ok             = flux_rec_focus_rect(b, &focus, outer * 0.5f);
	}
	if (ok) ok = rec_check_label(b, snap->u.check.label, bounds, state, t);
	return ok;
}
