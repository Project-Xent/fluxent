#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_color_picker_geom.h"
#include <math.h>

static FluxColor cp_hsv(float h, float s, float v) { return flux_color_fromhsv(h, s, v, 1.0f); }

static bool      cp_vgrad(XentDlBuilder *b, FluxRect const *r, FluxColor top, FluxColor bottom) {
	XentLinearGrad g = {r->x, r->y, r->x, r->y + r->h, 0.0f, 1.0f, flux_rec_xcolor(top), flux_rec_xcolor(bottom)};
	return xent_dl_fillrectgrad(b, flux_rec_xrect(r), &g);
}

static bool cp_hgrad(XentDlBuilder *b, FluxRect const *r, FluxColor left, FluxColor right) {
	XentLinearGrad g = {r->x, r->y, r->x + r->w, r->y, 0.0f, 1.0f, flux_rec_xcolor(left), flux_rec_xcolor(right)};
	return xent_dl_fillrectgrad(b, flux_rec_xrect(r), &g);
}

static bool cp_spectrum_columns(XentDlBuilder *b, FluxColorPickerSnapshot const *c, FluxRect const *sq) {
	int columns = ( int ) sq->w;
	for (int x = 0; x < columns; ++x) {
		float     f = ( float ) x / ( float ) (columns - 1);
		FluxColor top, bottom = flux_color_rgb(0, 0, 0);
		switch (c->spectrum) {
		case XTK_SPECTRUM_HUE_VALUE        : top = cp_hsv(f * FLUX_CP_MAX_HUE, c->saturation, 1.0f); break;
		case XTK_SPECTRUM_SATURATION_VALUE : top = cp_hsv(c->hue, f, 1.0f); break;
		default :
			top    = cp_hsv(f * FLUX_CP_MAX_HUE, 1.0f, 1.0f);
			bottom = flux_color_rgb(255, 255, 255);
			break;
		}
		FluxRect stripe = {sq->x + ( float ) x, sq->y, 1.0f, sq->h};
		if (!cp_vgrad(b, &stripe, top, bottom)) return false;
	}
	return true;
}

static bool cp_checkers(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *r, float radius) {
	FluxColor checker = rc && rc->is_dark ? flux_color_rgba(255, 255, 255, 0x19) : flux_color_rgba(0, 0, 0, 0x19);
	if (!xent_dl_pushcliprrect(b, flux_rec_xrect(r), radius)) return false;
	int cols = ( int ) ceilf(r->w / 4.0f), rows = ( int ) ceilf(r->h / 4.0f);
	for (int i = 0; i < cols * rows; ++i) {
		int x = i % cols, y = i / cols;
		if ((x + y) % 2 == 0) continue;
		FluxRect cell = {r->x + x * 4.0f, r->y + y * 4.0f, 4.0f, 4.0f};
		if (!flux_rec_fill_rect(b, &cell, checker)) return false;
	}
	return xent_dl_popclip(b);
}

static bool cp_border(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r) {
	FluxRect inner = {r->x + 1.0f, r->y + 1.0f, r->w - 2.0f, r->h - 2.0f};
	return flux_rec_stroke_rounded(b, &inner, FLUX_CORNER_RADIUS - 1.0f, t->ctrl_stroke_default, 2.0f);
}

static bool cp_ellipse_light(FluxColorPickerSnapshot const *c) {
	FluxColor shown = c->spectrum == XTK_SPECTRUM_HUE_SATURATION ? cp_hsv(c->hue, c->saturation, 1.0f) : c->color;
	uint32_t  rgba  = shown.rgba;
	double    lum   = 0.0;
	double    k [3] = {0.2126, 0.7152, 0.0722};
	for (int i = 0; i < 3; ++i) {
		double ch  = ( double ) (rgba >> (24 - 8 * i) & 255u);
		lum       += k [i] * (ch <= 10.0 ? ch / 3294.0 : pow(ch / 269.0 + 0.0513, 2.4));
	}
	return lum <= 0.5;
}

static FluxPoint cp_ellipse_at(FluxColorPickerSnapshot const *c, FluxRect const *sq) {
	float hp = fminf(fmaxf(c->hue, 0.0f), FLUX_CP_MAX_HUE) / FLUX_CP_MAX_HUE;
	float xp = c->spectrum == XTK_SPECTRUM_SATURATION_VALUE ? c->saturation : hp;
	float yp = 1.0f - (c->spectrum == XTK_SPECTRUM_HUE_SATURATION ? c->saturation : c->value);
	return (FluxPoint) {sq->x + sq->w * xp, sq->y + sq->h * yp};
}

static bool cp_selection(
  XentDlBuilder *b, FluxRenderSnap const *snap, FluxControlState const *state, FluxRect const *sq,
  FluxRect const *bounds
) {
	FluxColorPickerSnapshot const *c     = &snap->u.color_picker;
	FluxPoint                      at    = cp_ellipse_at(c, sq);
	bool                           light = cp_ellipse_light(c);
	float                          hx = snap->hover_local_x + bounds->x, hy = snap->hover_local_y + bounds->y;
	bool                           over = state->hovered && flux_rect_contains(sq, hx, hy);
	FluxColor                      ring = light ? flux_color_rgb(255, 255, 255) : flux_color_rgb(0, 0, 0);
	FluxColor                      halo = light ? flux_color_rgb(0, 0, 0) : flux_color_rgb(255, 255, 255);
	float                          size = c->large ? 48.0f : 16.0f;
	if (over && !c->large) ring.rgba = (ring.rgba & 0xffffff00u) | 204u;
	if (!xent_dl_pushcliprect(b, flux_rec_xrect(sq))) return false;
	bool ok = !state->focused || flux_rec_stroke_circle(b, at.x, at.y, size + 2.0f, halo, 2.0f);
	ok      = ok && flux_rec_stroke_circle(b, at.x, at.y, size - 2.0f, ring, 2.0f);
	return ok && xent_dl_popclip(b);
}

static bool cp_spectrum(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRenderSnap const *snap, FluxControlState const *state,
  FluxRect const *bounds
) {
	FluxRect sq = {bounds->x, bounds->y + FLUX_CP_PAD, FLUX_CP_SPECTRUM, FLUX_CP_SPECTRUM};
	bool     ok = xent_dl_pushcliprrect(b, flux_rec_xrect(&sq), FLUX_CORNER_RADIUS);
	ok          = ok && cp_spectrum_columns(b, &snap->u.color_picker, &sq) && xent_dl_popclip(b);
	ok          = ok && cp_border(b, t, &sq);
	return ok && cp_selection(b, snap, state, &sq, bounds);
}

static bool cp_preview(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxThemeColors const *t, FluxColorPickerSnapshot const *c,
  FluxRect const *bounds
) {
	FluxRect r  = {bounds->x + FLUX_CP_PREVIEW_X, bounds->y + FLUX_CP_PAD, FLUX_CP_PREVIEW_W, FLUX_CP_SPECTRUM};
	bool     ok = cp_checkers(b, rc, &r, FLUX_CORNER_RADIUS);
	ok          = ok && flux_rec_fill_rounded(b, &r, FLUX_CORNER_RADIUS, c->color);
	return ok && cp_border(b, t, &r);
}

static bool cp_hue_track(XentDlBuilder *b, FluxRect const *r) {
	for (int i = 0; i < 6; ++i) {
		float    h0 = i * 60.0f, h1 = i == 5 ? FLUX_CP_MAX_HUE : (i + 1) * 60.0f;
		float    x0 = r->x + r->w * h0 / FLUX_CP_MAX_HUE, x1 = r->x + r->w * h1 / FLUX_CP_MAX_HUE;
		FluxRect seg = {x0, r->y, x1 - x0, r->h};
		if (!cp_hgrad(b, &seg, cp_hsv(h0, 1.0f, 1.0f), cp_hsv(h1, 1.0f, 1.0f))) return false;
	}
	return true;
}

static bool cp_third_track(XentDlBuilder *b, FluxColorPickerSnapshot const *c, FluxRect const *r) {
	if (!xent_dl_pushcliprrect(b, flux_rec_xrect(r), 6.0f)) return false;
	bool ok;
	switch (c->spectrum) {
	case XTK_SPECTRUM_SATURATION_VALUE : ok = cp_hue_track(b, r); break;
	case XTK_SPECTRUM_HUE_VALUE : ok = cp_hgrad(b, r, cp_hsv(c->hue, 0.0f, 1.0f), cp_hsv(c->hue, 1.0f, 1.0f)); break;
	default : ok = cp_hgrad(b, r, cp_hsv(c->hue, c->saturation, 0.0f), cp_hsv(c->hue, c->saturation, 1.0f)); break;
	}
	return ok && xent_dl_popclip(b);
}

static bool
cp_alpha_track(XentDlBuilder *b, FluxRenderCtx const *rc, FluxColorPickerSnapshot const *c, FluxRect const *r) {
	if (!cp_checkers(b, rc, r, FLUX_CORNER_RADIUS)) return false;
	FluxColor clear = {c->color.rgba & 0xffffff00u}, opaque = {c->color.rgba | 255u};
	if (!xent_dl_pushcliprrect(b, flux_rec_xrect(r), 6.0f)) return false;
	return cp_hgrad(b, r, clear, opaque) && xent_dl_popclip(b);
}

static FluxColor cp_base(bool dark, uint8_t alpha) {
	return dark ? flux_color_rgba(255, 255, 255, alpha) : flux_color_rgba(0, 0, 0, alpha);
}

static bool cp_more(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxColorPickerSnapshot const *c, FluxControlState const *state,
  FluxRect const *bounds
) {
	if (!c->more_button) return true;
	FluxColorPickerGeom g    = flux_colorpicker_geom_of(c);
	bool                dark = rc && rc->is_dark;
	FluxRect  r = {bounds->x + FLUX_CP_CONTENT_W - FLUX_CP_MORE_W, bounds->y + g.more_y, FLUX_CP_MORE_W, FLUX_CP_ROW_H};
	FluxColor fg       = !state->enabled || c->more_pressed ? cp_base(dark, 0x66)
	                   : c->more_hover                      ? cp_base(dark, 0x99)
	                                                        : cp_base(dark, 0xff);
	FluxRect  content  = {r.x, r.y + 5.0f, r.w, r.h - 12.0f};
	FluxRect  glyph    = {content.x + content.w - 12.0f, content.y, 12.0f, content.h};
	FluxRect  label    = {content.x, content.y, content.w - 12.0f - 8.0f, content.h};
	XentTextStyle text = {.font_size = 14, .font_weight = 400, .h_align = 2, .v_align = 1};
	XentTextStyle icon = {
	  .font_size   = 12,
	  .font_weight = 400,
	  .h_align     = 1,
	  .v_align     = 1,
	  .flags       = XENT_DL_TEXT_ICON,
	  .font_family = flux_icon_font_family()};
	bool ok = flux_rec_text_ex(b, &label, c->opened ? "Less" : "More", &text, fg);
	ok      = ok && flux_rec_text_ex(b, &glyph, c->opened ? "\xEE\x9C\x8E" : "\xEE\x9C\x8D", &icon, fg);
	return ok && (!c->more_focus || flux_rec_focus_rect(b, flux_rec_theme(rc), &r, FLUX_CORNER_RADIUS));
}

static bool
cp_labels(XentDlBuilder *b, FluxThemeColors const *t, FluxColorPickerSnapshot const *c, FluxRect const *bounds) {
	FluxColorPickerGeom g = flux_colorpicker_geom_of(c);
	if (!g.entries) return true;
	XentTextStyle style = {.font_size = 14, .font_weight = 400, .v_align = 1};
	for (int row = 0; row < (c->alpha ? 4 : 3); ++row) {
		FluxRect r = {
		  bounds->x + FLUX_CP_LABEL_X, bounds->y + flux_colorpicker_row_y(&g, row), FLUX_CP_CONTENT_W - FLUX_CP_LABEL_X,
		  FLUX_CP_ROW_H};
		if (!flux_rec_text_ex(b, &r, flux_colorpicker_label(row, c->hsv), &style, t->text_primary)) return false;
	}
	return true;
}

XentDl *flux_colorpicker_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	FluxColorPickerSnapshot const *c = &snap->u.color_picker;
	FluxThemeColors const         *t = flux_rec_theme(rc);
	FluxColorPickerGeom            g = flux_colorpicker_geom_of(c);
	FluxRect third                   = {bounds->x, bounds->y + g.third_y + 10.0f, FLUX_CP_CONTENT_W, FLUX_CP_TRACK_H};
	FluxRect alpha                   = {bounds->x, bounds->y + g.alpha_y + 10.0f, FLUX_CP_CONTENT_W, FLUX_CP_TRACK_H};
	bool     ok                      = cp_spectrum(b, t, snap, state, bounds);
	ok                               = ok && cp_preview(b, rc, t, c, bounds);
	ok                               = ok && cp_third_track(b, c, &third);
	ok                               = ok && (!c->alpha || cp_alpha_track(b, rc, c, &alpha));
	ok                               = ok && cp_labels(b, t, c, bounds);
	ok                               = ok && cp_more(b, rc, c, state, bounds);
	return flux_rec_finish(b, ok);
}

bool flux_colorpicker_slider_record(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (bounds->w <= 0.0f || bounds->h <= 0.0f) return true;
	FluxSliderSnapshot const *s     = &snap->u.slider;
	FluxThemeColors const    *t     = flux_rec_theme(rc);
	float                     value = s->thumb_held ? s->slider_intermediate : s->current_value;
	float                     frac  = (value - s->min_value) / fmaxf(1.0f, s->max_value - s->min_value);
	float                     cx    = bounds->x + FLUX_CP_THUMB * 0.5f + fmaxf(0.0f, bounds->w - FLUX_CP_THUMB) * frac;
	float                     cy    = bounds->y + bounds->h * 0.5f;
	FluxRect  outer = {cx - FLUX_CP_THUMB * 0.5f, cy - FLUX_CP_THUMB * 0.5f, FLUX_CP_THUMB, FLUX_CP_THUMB};
	bool      dark  = rc && rc->is_dark;
	FluxColor inner = !state->enabled ? t->ctrl_strong_fill_disabled
	                : state->hovered && !state->pressed
	                  ? (dark ? flux_color_rgb(0xf2, 0xf2, 0xf2) : flux_color_rgb(0x17, 0x17, 0x17))
	                  : t->text_primary;
	bool      ok    = flux_rec_fill_rounded(b, &outer, FLUX_CP_THUMB * 0.5f, t->ctrl_solid_fill_default);
	ok              = ok && flux_rec_elevation_border(b, rc, &outer, FLUX_CP_THUMB * 0.5f, false);
	ok              = ok && flux_rec_fill_circle(b, cx, cy, 10.0f, inner);
	return ok && (!state->focused || flux_rec_focus_rect(b, t, bounds, FLUX_CORNER_RADIUS));
}
