#include "controls/draw/flux_control_record_internal.h"

#include "render/flux_anim.h"

static bool rec_check_label(
  XentDlBuilder *b, char const *label, FluxRect const *bounds, FluxControlState const *state, FluxThemeColors const *t
) {
	if (!label || !label [0]) return true;
	FluxColor     color = state->enabled ? t->text_primary : t->text_disabled;
	float         inset = FLUX_CHECKBOX_SIZE + 8.0f;
	FluxRect      tr    = {bounds->x + inset, bounds->y + 5.0f, flux_maxf(0.0f, bounds->w - inset), bounds->h - 5.0f};
	XentTextStyle style
	  = {.font_size = 14.0f, .font_weight = 400u, .h_align = 0u, .v_align = 0u, .flags = XENT_DL_TEXT_WRAP};
	return flux_rec_text_ex(b, &tr, label, &style, color);
}

typedef struct CheckboxPalette {
	FluxColor fill;
	FluxColor stroke;
	FluxColor glyph;
} CheckboxPalette;

static CheckboxPalette checkbox_palette(FluxThemeColors const *t, FluxControlState const *state, bool on) {
	if (!state->enabled)
		return on ? (CheckboxPalette) {t->accent_disabled, t->ctrl_strong_stroke_disabled, t->text_on_accent_disabled}
		          : (CheckboxPalette) {
					  t->ctrl_alt_fill_disabled, t->ctrl_strong_stroke_disabled, t->text_on_accent_disabled};
	if (on) {
		if (state->pressed)
			return (CheckboxPalette) {t->accent_tertiary, t->accent_tertiary, t->text_on_accent_secondary};
		if (state->hovered)
			return (CheckboxPalette) {t->accent_secondary, t->accent_secondary, t->text_on_accent_primary};
		return (CheckboxPalette) {t->accent_default, t->accent_default, t->text_on_accent_primary};
	}
	if (state->pressed)
		return (CheckboxPalette) {
		  t->ctrl_alt_fill_quarternary, t->ctrl_strong_stroke_disabled, t->text_on_accent_primary};
	if (state->hovered)
		return (CheckboxPalette) {t->ctrl_alt_fill_tertiary, t->ctrl_strong_stroke_default, t->text_on_accent_primary};
	return (CheckboxPalette) {t->ctrl_alt_fill_secondary, t->ctrl_strong_stroke_default, t->text_on_accent_primary};
}

#define CHECK_PATH_SCALE (0.7f * 1.05f * FLUX_CHECKBOX_SIZE / 48.0f)
#define CHECK_STROKE     (4.0f * CHECK_PATH_SCALE)

static float check_map(float v, float origin) {
	float canvas = (v * 0.7f + origin - 24.0f) * 1.05f + 24.0f;
	return canvas * FLUX_CHECKBOX_SIZE / 48.0f;
}

static bool
check_draw_trim(XentDlBuilder *b, FluxRect const *box, float const pts [3][2], float from, float to, FluxColor c) {
	float x [3], y [3];
	for (int i = 0; i < 3; ++i) {
		x [i] = box->x + check_map(pts [i][0], 24.0f);
		y [i] = box->y + check_map(pts [i][1], 23.0f) + 1.0f;
	}
	float     l0 = hypotf(x [1] - x [0], y [1] - y [0]);
	float     l1 = hypotf(x [2] - x [1], y [2] - y [1]);
	float     s0 = from * (l0 + l1), s1 = to * (l0 + l1);
	XentColor xc = flux_rec_xcolor(c);
	for (int seg = 0; seg < 2; ++seg) {
		float base = seg ? l0 : 0.0f, len = seg ? l1 : l0;
		float a = flux_clamp01((s0 - base) / len), e = flux_clamp01((s1 - base) / len);
		if (e <= a) continue;
		float ax = x [seg] + (x [seg + 1] - x [seg]) * a, ay = y [seg] + (y [seg + 1] - y [seg]) * a;
		float ex = x [seg] + (x [seg + 1] - x [seg]) * e, ey = y [seg] + (y [seg + 1] - y [seg]) * e;
		if (!xent_dl_strokeline(b, ax, ay, ex, ey, xc, CHECK_STROKE, XENT_STROKE_CAP_ROUND)) return false;
	}
	return true;
}

static bool check_draw_dash(XentDlBuilder *b, FluxRect const *box, FluxColor c) {
	float y = box->y + check_map(-0.125f, 24.0f);
	return xent_dl_strokeline(
	  b, box->x + check_map(-11.75f, 24.0f), y, box->x + check_map(11.875f, 24.0f), y, flux_rec_xcolor(c), CHECK_STROKE,
	  XENT_STROKE_CAP_ROUND
	);
}

static float ease_accept_on(float t) { return flux_cubic_bezier(t, 0.55f, 0.0f, 0.0f, 1.0f); }

static float ease_accept_off(float t) { return flux_cubic_bezier(t, 0.167f, 0.167f, 0.833f, 0.833f); }

static void  check_trim(FluxRenderCtx const *rc, FluxRenderSnap const *snap, bool on, float *from, float *to) {
	FluxCacheEntry *ce = rc && rc->cache ? flux_rcache_ensure(rc->cache, snap->id) : NULL;
	if (!ce) {
		*from = 0.0f;
		*to   = on ? 1.0f : 0.0f;
		return;
	}
	FluxTween *tw = &ce->check_anim;
	tw->easing    = on ? ease_accept_on : ease_accept_off;
	float v;
	flux_rec_note_animating(rc, flux_tween_update(tw, on ? 1.0f : 0.0f, on ? 19.0 / 60.0 : 4.0 / 60.0, rc->now, &v));
	*from = on ? 0.0f : 1.0f - v;
	*to   = on ? v : 1.0f;
}

static FluxRect checkbox_focus_bounds(FluxRect const *bounds) {
	return (FluxRect) {bounds->x - 4.0f, bounds->y, bounds->w + 8.0f, bounds->h};
}

bool flux_rec_checkbox_box(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *box,
  FluxControlState const *state, FluxCheckState cs
) {
	static float const check [3][2] = {
	  {-15.172f, 0.016f  },
      {-5.0f,    10.188f },
      {15.337f,  -10.337f}
    };
	CheckboxPalette pal  = checkbox_palette(flux_rec_theme(rc), state, cs != FLUX_CHECK_UNCHECKED);
	float           from = 0.0f, to = 0.0f;
	check_trim(rc, snap, cs == FLUX_CHECK_CHECKED, &from, &to);

	bool ok = flux_rec_fill_rounded(b, box, FLUX_CHECKBOX_CORNER, pal.fill);
	if (ok) ok = flux_rec_stroke_rounded(b, box, FLUX_CHECKBOX_CORNER, pal.stroke, 1.0f);
	if (ok && cs == FLUX_CHECK_INDETERMINATE) ok = check_draw_dash(b, box, pal.glyph);
	else if (ok && to > from) ok = check_draw_trim(b, box, check, from, to, pal.glyph);
	return ok;
}

bool flux_rec_build_checkbox(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t   = flux_rec_theme(rc);
	float                  cy  = bounds->y + bounds->h * 0.5f;
	FluxRect               box = {bounds->x, cy - FLUX_CHECKBOX_SIZE * 0.5f, FLUX_CHECKBOX_SIZE, FLUX_CHECKBOX_SIZE};
	bool                   ok  = flux_rec_checkbox_box(b, rc, snap, &box, state, snap->u.check.check_state);
	if (ok && state->focused && state->enabled) {
		FluxRect fr = checkbox_focus_bounds(bounds);
		ok          = flux_rec_focus_rect(b, t, &fr, FLUX_CHECKBOX_CORNER);
	}
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
	if (ok && state->focused && state->enabled)
		ok = flux_rec_focus_rect(b, flux_rec_theme(rc), &track, FLUX_TOGGLE_HEIGHT * 0.5f);
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
		ok             = flux_rec_focus_rect(b, flux_rec_theme(rc), &focus, outer * 0.5f);
	}
	if (ok) ok = rec_check_label(b, snap->u.check.label, bounds, state, t);
	return ok;
}
