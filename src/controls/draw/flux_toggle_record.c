#include "controls/draw/flux_control_record_internal.h"

#include "render/flux_anim.h"

static bool rec_check_label(XentDlBuilder *b, char const *label, FluxRect const *bounds, FluxColor color) {
	if (!label || !label [0]) return true;
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

static CheckboxPalette checkbox_contrast_checked(FluxContrastColors const *p, FluxControlState const *state) {
	if (state->pressed) return (CheckboxPalette) {p->button_face, p->button_face, p->button_text};
	if (state->hovered) return (CheckboxPalette) {p->button_text, p->button_text, p->button_face};
	return (CheckboxPalette) {p->highlight, p->highlight, p->highlight_text};
}

static CheckboxPalette
checkbox_contrast(FluxContrastColors const *p, FluxControlState const *state, FluxCheckState cs) {
	if (!state->enabled)
		return cs == FLUX_CHECK_UNCHECKED ? (CheckboxPalette) {p->window, p->gray_text, p->gray_text}
		                                  : (CheckboxPalette) {p->gray_text, p->gray_text, p->window};
	if (cs == FLUX_CHECK_CHECKED) return checkbox_contrast_checked(p, state);
	if (cs == FLUX_CHECK_UNCHECKED && !state->pressed && !state->hovered)
		return (CheckboxPalette) {p->button_face, p->button_text, p->button_face};
	if (state->hovered && !state->pressed) return (CheckboxPalette) {p->highlight_text, p->highlight, p->highlight};
	return (CheckboxPalette) {p->highlight, p->highlight, p->highlight_text};
}

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

static bool check_draw_trim(
  XentDlBuilder *b, FluxRect const *box, float const pts [3][2], bool rtl, float from, float to, FluxColor c
) {
	float x [3], y [3];
	float direction = rtl ? -1.0f : 1.0f;
	for (int i = 0; i < 3; ++i) {
		x [i] = box->x + check_map(direction * pts [i][0], 24.0f);
		y [i] = box->y + check_map(pts [i][1], 23.0f) + 1.0f;
	}
	float       l0         = hypotf(x [1] - x [0], y [1] - y [0]);
	float       l1         = hypotf(x [2] - x [1], y [2] - y [1]);
	float const starts [2] = {0.0f, l0}, lengths [2] = {l0, l1};
	float       s0 = from * (l0 + l1), s1 = to * (l0 + l1);
	XentColor   xc = flux_rec_xcolor(c);
	for (int seg = 0; seg < 2; ++seg) {
		float base = starts [seg], len = lengths [seg];
		float a = flux_clamp01((s0 - base) / len), e = flux_clamp01((s1 - base) / len);
		if (e <= a) continue;
		float ax = x [seg] + (x [seg + 1] - x [seg]) * a, ay = y [seg] + (y [seg + 1] - y [seg]) * a;
		float ex = x [seg] + (x [seg + 1] - x [seg]) * e, ey = y [seg] + (y [seg + 1] - y [seg]) * e;
		if (!xent_dl_strokeline(b, ax, ay, ex, ey, xc, CHECK_STROKE, XENT_STROKE_CAP_ROUND)) return false;
	}
	return true;
}

static bool check_draw_dash(XentDlBuilder *b, FluxRect const *box, bool rtl, FluxColor c) {
	float y         = box->y + check_map(-0.125f, 24.0f);
	float direction = rtl ? -1.0f : 1.0f;
	return xent_dl_strokeline(
	  b, box->x + check_map(direction * -11.75f, 24.0f), y, box->x + check_map(direction * 11.875f, 24.0f), y,
	  flux_rec_xcolor(c), CHECK_STROKE, XENT_STROKE_CAP_ROUND
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
	FluxThemeColors const *t    = flux_rec_theme(rc);
	CheckboxPalette        pal  = t->contrast.active ? checkbox_contrast(&t->contrast, state, cs)
	                                                 : checkbox_palette(t, state, cs != FLUX_CHECK_UNCHECKED);
	float                  from = 0.0f, to = 0.0f;
	check_trim(rc, snap, cs == FLUX_CHECK_CHECKED, &from, &to);

	bool ok = flux_rec_fill_rounded(b, box, FLUX_CHECKBOX_CORNER, pal.fill);
	if (ok) ok = flux_rec_stroke_rounded(b, box, FLUX_CHECKBOX_CORNER, pal.stroke, 1.0f);
	if (ok && cs == FLUX_CHECK_INDETERMINATE) ok = check_draw_dash(b, box, snap->rtl, pal.glyph);
	else if (ok && to > from) ok = check_draw_trim(b, box, check, snap->rtl, from, to, pal.glyph);
	return ok;
}

bool flux_rec_build_checkbox(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t   = flux_rec_theme(rc);
	float                  cy  = bounds->y + bounds->h * 0.5f;
	FluxRect               box = {bounds->x, cy - FLUX_CHECKBOX_SIZE * 0.5f, FLUX_CHECKBOX_SIZE, FLUX_CHECKBOX_SIZE};
	FluxCheckState         cs  = snap->u.check.check_state;
	bool                   ok  = (!t->contrast.active || flux_rec_fill_rect(b, bounds, t->contrast.window))
	                          && flux_rec_checkbox_box(b, rc, snap, &box, state, cs);
	if (ok && state->focused && state->enabled) {
		FluxRect fr = checkbox_focus_bounds(bounds);
		ok          = flux_rec_focus_rect(b, t, &fr, FLUX_CHECKBOX_CORNER);
	}
	FluxColor text = t->text_primary;
	if (t->contrast.active && cs != FLUX_CHECK_INDETERMINATE) text = t->contrast.button_text;
	if (!state->enabled) text = t->text_disabled;
	if (ok) ok = rec_check_label(b, snap->u.check.label, bounds, text);
	return ok;
}

static bool switch_draw_track(
  XentDlBuilder *b, FluxRect const *track, FluxThemeColors const *t, bool enabled, FluxRecStateAnim anim
) {
	FluxColor off_n = t->ctrl_alt_fill_secondary, off_h = t->ctrl_alt_fill_tertiary,
			  off_p = t->ctrl_alt_fill_quarternary;
	FluxColor on_n = t->accent_default, on_h = t->accent_secondary, on_p = t->accent_tertiary;
	if (!enabled) {
		off_n = off_h = off_p = t->ctrl_alt_fill_disabled;
		on_n = on_h = on_p = t->accent_disabled;
	}
	FluxColor off_c      = flux_anim_lerp_color(flux_anim_lerp_color(off_n, off_h, anim.hover), off_p, anim.press);
	FluxColor on_c       = flux_anim_lerp_color(flux_anim_lerp_color(on_n, on_h, anim.hover), on_p, anim.press);
	FluxColor track_fill = flux_anim_lerp_color(off_c, on_c, anim.check);

	if (!flux_rec_fill_rounded(b, track, track->h * 0.5f, track_fill)) return false;
	if (anim.check >= 0.5f) return true;
	FluxColor off_stroke = enabled ? t->ctrl_strong_stroke_default : t->ctrl_strong_stroke_disabled;
	if (anim.check > 0.0f) {
		float   fade    = 1.0f - anim.check * 2.0f;
		uint8_t sa      = ( uint8_t ) (flux_color_af(off_stroke) * fade * 255.0f);
		off_stroke.rgba = (off_stroke.rgba & 0xffffff00u) | sa;
	}
	return flux_rec_stroke_rounded(b, track, track->h * 0.5f, off_stroke, 1.0f);
}

static bool switch_draw_knob(
  XentDlBuilder *b, FluxRect const *track, FluxColor color, bool enabled, bool contrast, FluxRecStateAnim anim
) {
	float off_x   = track->x + track->h * 0.5f;
	float on_x    = track->x + track->w - track->h * 0.5f;
	float thumb_x = flux_anim_mixf(off_x, on_x, anim.check);
	if (contrast) thumb_x -= 0.5f * (1.0f - anim.hover * (1.0f - anim.press));
	else if (enabled) thumb_x += (1.0f - 2.0f * anim.check) * FLUX_TOGGLE_TRAVEL_EXT * anim.press;

	float knob_w = FLUX_TOGGLE_KNOB_NORMAL;
	float knob_h = FLUX_TOGGLE_KNOB_NORMAL;
	if (enabled) {
		knob_w = FLUX_TOGGLE_KNOB_NORMAL
		       + (FLUX_TOGGLE_KNOB_HOVER - FLUX_TOGGLE_KNOB_NORMAL) * anim.hover
		       + (FLUX_TOGGLE_KNOB_PRESS_W - FLUX_TOGGLE_KNOB_HOVER) * anim.press;
		knob_h = FLUX_TOGGLE_KNOB_NORMAL + (FLUX_TOGGLE_KNOB_HOVER - FLUX_TOGGLE_KNOB_NORMAL) * anim.hover;
		if (contrast) {
			knob_w = flux_anim_mixf(knob_h, FLUX_TOGGLE_KNOB_PRESS_W, anim.press);
			knob_h = flux_anim_mixf(knob_h, FLUX_TOGGLE_KNOB_PRESS_H, anim.press);
		}
	}
	FluxRect knob = {thumb_x - knob_w * 0.5f, track->y + (track->h - knob_h) * 0.5f, knob_w, knob_h};
	return flux_rec_fill_rounded(b, &knob, flux_minf(knob.w, knob.h) * 0.5f, color);
}

typedef struct TogglePalette {
	FluxColor fill, stroke, knob;
} TogglePalette;

static TogglePalette switch_contrast(FluxContrastColors const *p, FluxControlState const *state, bool on) {
	if (!state->enabled) return (TogglePalette) {p->window, p->gray_text, p->gray_text};
	if (state->hovered || state->pressed) {
		if (on) return (TogglePalette) {p->button_face, p->button_text, p->button_text};
		return (TogglePalette) {p->highlight_text, p->highlight, p->highlight};
	}
	if (on) return (TogglePalette) {p->highlight, p->highlight, p->highlight_text};
	return (TogglePalette) {p->button_face, p->button_text, p->button_text};
}

static bool switch_draw(
  XentDlBuilder *b, FluxRect const *track, FluxThemeColors const *t, FluxControlState const *state, bool on,
  FluxRecStateAnim anim
) {
	if (t->contrast.active) {
		TogglePalette p = switch_contrast(&t->contrast, state, on);
		return flux_rec_fill_rounded(b, track, track->h * 0.5f, p.fill)
		    && flux_rec_stroke_rounded(b, track, track->h * 0.5f, p.stroke, 1.0f)
		    && switch_draw_knob(b, track, p.knob, state->enabled, true, anim);
	}
	FluxColor off      = state->enabled ? t->text_secondary : t->text_disabled;
	FluxColor on_color = state->enabled ? t->text_on_accent_primary : t->text_on_accent_disabled;
	FluxColor knob     = flux_anim_lerp_color(off, on_color, anim.check);
	return switch_draw_track(b, track, t, state->enabled, anim)
	    && switch_draw_knob(b, track, knob, state->enabled, false, anim);
}

static bool toggle_contrast_focus(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds) {
	FluxRect focus = {bounds->x - 7.0f, bounds->y - 3.0f, bounds->w + 14.0f, bounds->h + 6.0f};
	return flux_rec_focus_rect(b, t, &focus, FLUX_CORNER_RADIUS);
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
	if (!switch_draw(b, &track, t, state, on, anim)) return false;
	if (!state->focused || !state->enabled) return true;
	if (t->contrast.active) return toggle_contrast_focus(b, t, bounds);
	return flux_rec_focus_rect(b, t, &track, track.h * 0.5f);
}

static TogglePalette radio_contrast_checked(FluxContrastColors const *p, FluxControlState const *state) {
	if (state->pressed) return (TogglePalette) {p->button_face, p->button_face, p->button_text};
	if (state->hovered) return (TogglePalette) {p->button_face, p->button_text, p->button_text};
	return (TogglePalette) {p->highlight_text, p->highlight, p->highlight};
}

static TogglePalette radio_contrast(FluxContrastColors const *p, FluxControlState const *state, bool checked) {
	if (!state->enabled) return (TogglePalette) {p->window, p->gray_text, p->gray_text};
	if (checked) return radio_contrast_checked(p, state);
	if (state->pressed) return (TogglePalette) {p->highlight_text, p->highlight_text, p->highlight};
	if (state->hovered) return (TogglePalette) {p->highlight_text, p->highlight, p->highlight};
	return (TogglePalette) {p->button_face, p->button_text, p->highlight};
}

static float radio_contrast_glyph_size(FluxControlState const *state, bool checked, FluxRecStateAnim anim) {
	if (!state->enabled) return checked ? FLUX_RADIO_GLYPH_HOVER * 2.0f : 0.0f;
	if (!checked) return state->pressed ? flux_anim_mixf(4.0f, FLUX_RADIO_PRESS_GLYPH, anim.press) : 0.0f;
	float radius = flux_anim_mixf(FLUX_RADIO_GLYPH, FLUX_RADIO_GLYPH_HOVER, anim.hover);
	return flux_anim_mixf(radius, FLUX_RADIO_GLYPH_PRESSED, anim.press) * 2.0f;
}

static bool radio_draw_circle(XentDlBuilder *b, float cx, float cy, TogglePalette p) {
	return flux_rec_fill_circle(b, cx, cy, FLUX_RADIO_SIZE, p.fill)
	    && flux_rec_stroke_circle(b, cx, cy, FLUX_RADIO_SIZE, p.stroke, 1.0f);
}

static bool radio_draw_contrast(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state, FluxRecStateAnim anim
) {
	bool          checked = snap->u.check.check_state == FLUX_CHECK_CHECKED;
	TogglePalette p       = radio_contrast(&t->contrast, state, checked);
	float         cx = bounds->x + FLUX_RADIO_SIZE * 0.5f, cy = bounds->y + 16.0f;
	float         glyph = radio_contrast_glyph_size(state, checked, anim);
	if (!radio_draw_circle(b, cx, cy, p)) return false;
	if (!flux_rec_fill_circle(b, cx, cy, glyph, p.knob)) return false;
	if (state->focused && state->enabled && !toggle_contrast_focus(b, t, bounds)) return false;
	FluxColor text = state->enabled ? t->contrast.button_text : t->contrast.gray_text;
	return rec_check_label(b, snap->u.check.label, bounds, text);
}

static TogglePalette radio_palette(FluxThemeColors const *t, bool enabled, bool checked, FluxRecStateAnim anim) {
	if (!enabled) {
		FluxColor glyph = checked ? t->text_on_accent_disabled : (FluxColor) {0};
		return (TogglePalette) {t->ctrl_alt_fill_disabled, t->ctrl_strong_stroke_disabled, glyph};
	}
	if (checked) {
		FluxColor fill = flux_anim_lerp_color(
		  flux_anim_lerp_color(t->accent_default, t->accent_secondary, anim.hover), t->accent_tertiary, anim.press
		);
		return (TogglePalette) {fill, fill, t->text_on_accent_primary};
	}
	FluxColor stroke = flux_anim_lerp_color(t->ctrl_strong_stroke_default, t->ctrl_strong_stroke_disabled, anim.press);
	FluxColor fill   = flux_anim_lerp_color(
	  flux_anim_lerp_color(t->ctrl_alt_fill_secondary, t->ctrl_alt_fill_tertiary, anim.hover),
	  t->ctrl_alt_fill_quarternary, anim.press
	);
	return (TogglePalette) {fill, stroke, {0}};
}

static float radio_glyph_size(FluxRecStateAnim anim) {
	float radius = flux_anim_mixf(
	  flux_anim_mixf(FLUX_RADIO_GLYPH, FLUX_RADIO_GLYPH_HOVER, anim.hover), FLUX_RADIO_GLYPH_PRESSED, anim.press
	);
	return radius * 2.0f * flux_ease_out_quad(anim.check);
}

static bool
radio_draw_glyphs(XentDlBuilder *b, FluxRect const *circle, TogglePalette p, FluxRecStateAnim anim, FluxColor preview) {
	float cx = circle->x + circle->w * 0.5f, cy = circle->y + circle->h * 0.5f;
	if (anim.check > 0.01f && !flux_rec_fill_circle(b, cx, cy, radio_glyph_size(anim), p.knob)) return false;
	if (anim.press <= 0.01f) return true;
	return flux_rec_fill_circle(b, cx, cy, FLUX_RADIO_PRESS_GLYPH * anim.press, preview);
}

bool flux_rec_build_radio(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t     = flux_rec_theme(rc);
	float                  outer = FLUX_RADIO_SIZE;
	float                  cy = bounds->y + bounds->h * 0.5f, cx = bounds->x + outer * 0.5f;
	bool                   checked = snap->u.check.check_state == FLUX_CHECK_CHECKED;
	FluxRecStateAnim       anim    = flux_rec_animstate(rc, snap, state, checked);
	if (t->contrast.active) return radio_draw_contrast(b, t, snap, bounds, state, anim);
	TogglePalette p = radio_palette(t, state->enabled, checked, anim);
	if (!radio_draw_circle(b, cx, cy, p)) return false;
	FluxRect  circle  = {bounds->x, cy - outer * 0.5f, outer, outer};
	FluxColor preview = !checked && state->enabled ? t->text_on_accent_primary : (FluxColor) {0};
	if (!radio_draw_glyphs(b, &circle, p, anim, preview)) return false;
	if (state->focused && state->enabled && !flux_rec_focus_rect(b, t, &circle, outer * 0.5f)) return false;
	return rec_check_label(b, snap->u.check.label, bounds, state->enabled ? t->text_primary : t->text_disabled);
}
