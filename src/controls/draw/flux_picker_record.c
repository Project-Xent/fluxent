#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_control_draw.h"

#include "render/flux_picker_geom.h"

#define PICKER_BORDER      1.0f
#define PICKER_TEXT_TOP    4.0f
#define PICKER_TEXT_H      20.0f
#define PICKER_MONTH_INSET 10.0f
#define PICKER_LEFT_INSET  12.0f

static float     picker_header_h(FluxRenderSnap const *snap) { return snap->u.picker.header_h; }

static FluxColor picker_text_color(FluxThemeColors const *t, FluxControlState const *state, bool has_value) {
	if (!state->enabled) return t->text_disabled;
	if (state->pressed) return t->text_secondary;
	return has_value || state->hovered ? t->text_primary : t->text_secondary;
}

static FluxRecButtonColors picker_colors(FluxThemeColors const *t, FluxControlState const *state) {
	FluxRecButtonColors c = {.fill = t->ctrl_fill_default, .stroke = t->ctrl_stroke_default, .use_elevation = true};
	if (!state->enabled) {
		c.fill          = t->ctrl_fill_disabled;
		c.use_elevation = false;
		return c;
	}
	if (state->pressed) {
		c.fill          = t->ctrl_fill_tertiary;
		c.use_elevation = false;
	}
	else if (state->hovered) c.fill = t->ctrl_fill_secondary;
	return c;
}

static bool
picker_draw_column(XentDlBuilder *b, FluxRect const *col, FluxRenderSnap const *snap, int i, FluxColor color) {
	char const *text = snap->u.picker.text [i];
	if (!text [0]) return true;

	FluxRect      area  = {col->x, col->y, col->w, col->h};
	XentTextStyle style = {.font_size = FLUX_FONT_SIZE_DEFAULT, .font_weight = 400u, .v_align = 1u};
	if (snap->u.picker.left [i]) {
		float inset  = snap->u.picker.columns > 1 ? PICKER_MONTH_INSET : PICKER_LEFT_INSET;
		area.x      += inset;
		area.w      -= inset;
	}
	else style.h_align = 1u;
	if (area.w <= 0.0f) return true;
	if (!xent_dl_pushcliprect(b, flux_rec_xrect(col))) return false;
	bool ok = flux_rec_text_ex(b, &area, text, &style, color);
	return xent_dl_popclip(b) && ok;
}

static bool picker_draw_spacer(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *button, float x) {
	FluxRect spacer = {x, button->y + PICKER_BORDER, FLUX_PFLY_SPACER, button->h - 2.0f * PICKER_BORDER};
	return flux_rec_fill_rect(b, &spacer, t->ctrl_stroke_default);
}

static bool picker_draw_columns(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *button, FluxRenderSnap const *snap,
  FluxControlState const *state
) {
	int       n     = snap->u.picker.columns;
	float     total = 0.0f;
	FluxColor color = picker_text_color(t, state, snap->u.picker.has_value);
	for (int i = 0; i < n; i++) total += snap->u.picker.weight [i];
	if (n <= 0 || total <= 0.0f) return true;

	float room = button->w - 2.0f * PICKER_BORDER - ( float ) (n - 1) * FLUX_PFLY_SPACER;
	float x    = button->x + PICKER_BORDER;
	for (int i = 0; i < n; i++) {
		float    w   = room * snap->u.picker.weight [i] / total;
		FluxRect col = {x, button->y + PICKER_TEXT_TOP, w, snap->u.picker.button_h - 11.0f};
		if (!picker_draw_column(b, &col, snap, i, color)) return false;
		x += w;
		if (i + 1 < n && !picker_draw_spacer(b, t, button, x)) return false;
		x += FLUX_PFLY_SPACER;
	}
	return true;
}

XentDl *flux_picker_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;

	FluxThemeColors const *t        = flux_rec_theme(rc);
	float                  header_h = picker_header_h(snap);
	FluxRect               button   = {bounds->x, bounds->y + header_h, bounds->w, bounds->h - header_h};
	bool                   ok       = true;

	if (header_h > 0.0f) {
		FluxRect      head = {bounds->x, bounds->y, bounds->w, header_h - 4.0f};
		XentTextStyle style
		  = {.font_size = FLUX_FONT_SIZE_DEFAULT, .font_weight = 400u, .v_align = 0u, .flags = XENT_DL_TEXT_WRAP};
		ok = flux_rec_text_ex(
		  b, &head, snap->u.picker.header, &style, state->enabled ? t->text_primary : t->text_disabled
		);
	}
	FluxControlState button_state  = *state;
	button_state.hovered          &= snap->u.picker.pointer_on_button;
	button_state.pressed          &= snap->u.picker.press_on_button;
	if (snap->u.picker.open) button_state.pressed = button_state.hovered = false;
	state                      = &button_state;
	FluxRecButtonColors colors = picker_colors(t, state);
	if (ok) ok = flux_rec_button_chrome(b, rc, NULL, NULL, &button, FLUX_CORNER_RADIUS, &colors);
	if (ok) ok = picker_draw_columns(b, t, &button, snap, state);
	if (ok && state->focused && state->enabled) ok = flux_rec_focus_rect(b, t, &button, FLUX_CORNER_RADIUS);
	return flux_rec_finish(b, ok);
}

XentDl *flux_pickercard_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;

	FluxThemeColors const *t    = flux_rec_theme(rc);
	FluxPickerCardData     card = snap->u.picker_card;
	FluxPickerColumn       cols [FLUX_PICKER_COLUMNS];

	FluxRect               inset = {bounds->x + 0.5f, bounds->y + 0.5f, bounds->w - 1.0f, bounds->h - 1.0f};
	bool                   ok    = flux_rec_fill_rounded(b, bounds, FLUX_PFLY_RADIUS, t->flyout_background);
	if (ok) ok = flux_rec_stroke_rounded(b, &inset, FLUX_PFLY_RADIUS, t->flyout_border, FLUX_PFLY_BORDER);

	FluxRect band  = flux_pgeom_band(&card, bounds->w);
	band.x        += bounds->x;
	band.y        += bounds->y;
	if (ok) ok = flux_rec_fill_rounded(b, &band, FLUX_PFLY_RADIUS, t->accent_default);

	flux_pgeom_split(&card, bounds->w - 2.0f * FLUX_PFLY_BORDER, cols);
	for (int i = 0; ok && i + 1 < card.columns; i++) {
		FluxRect spacer
		  = {bounds->x + cols [i].x + cols [i].w, bounds->y + FLUX_PFLY_BORDER, FLUX_PFLY_SPACER, card.body_h};
		ok = flux_rec_fill_rect(b, &spacer, t->divider_stroke_default);
	}
	FluxRect rule = {
	  bounds->x + FLUX_PFLY_BORDER, bounds->y + FLUX_PFLY_BORDER + card.body_h, bounds->w - 2.0f * FLUX_PFLY_BORDER,
	  FLUX_PFLY_SPACER};
	if (ok) ok = flux_rec_fill_rect(b, &rule, t->divider_stroke_default);
	return flux_rec_finish(b, ok);
}
