#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_control_draw.h"

#include "render/flux_looping_geom.h"

#include <math.h>

#define LOOP_GLYPH_SIZE     8.0f
#define LOOP_GLYPH_PRESSED  7.0f

#define LOOP_ITEM_MARGIN_X  4.0f
#define LOOP_ITEM_MARGIN_Y  2.0f
#define LOOP_ITEM_CONTENT_X 2.0f

static FluxRect looping_deflate(FluxRect const *r, float dx, float dy) {
	return (FluxRect) {r->x + dx, r->y + dy, r->w - 2.0f * dx, r->h - 2.0f * dy};
}

static bool looping_rec_button(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r, wchar_t const *glyph, bool hovered, bool pressed
) {
	if (!flux_rec_fill_rect(b, r, t->flyout_background)) return false;
	FluxColor fg = (hovered || pressed) ? t->text_primary : t->text_secondary;
	return flux_rec_glyph_wc(b, r, glyph, pressed ? LOOP_GLYPH_PRESSED : LOOP_GLYPH_SIZE, fg);
}

XentDl *flux_looping_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	bool focus_ok = true;
	if (state->focused && state->enabled)
		focus_ok = flux_rec_focus_rect(b, flux_rec_theme(rc), bounds, FLUX_CORNER_RADIUS);
	if (!focus_ok || !snap->u.looping.buttons_shown) return flux_rec_finish(b, focus_ok);

	FluxThemeColors const *t    = flux_rec_theme(rc);
	FluxRect               up   = flux_looping_up_rect(bounds);
	FluxRect               down = flux_looping_down_rect(bounds);
	float                  hx   = state->hovered ? snap->hover_local_x : -1.0f;
	float                  hy   = state->hovered ? snap->hover_local_y : -1.0f;

	bool ok = looping_rec_button(b, t, &up, L"\xEDDB", flux_rect_contains(&up, hx, hy), snap->u.looping.up_pressed);
	if (!ok) return flux_rec_finish(b, false);
	ok = looping_rec_button(b, t, &down, L"\xEDDC", flux_rect_contains(&down, hx, hy), snap->u.looping.down_pressed);
	return flux_rec_finish(b, ok);
}

static FluxColor looping_item_text(FluxThemeColors const *t, FluxControlState const *state) {
	return (state->enabled && state->pressed) ? t->text_secondary : t->text_primary;
}

static bool
looping_item_label(XentDlBuilder *b, FluxRect const *content, FluxRenderSnap const *snap, float size, FluxColor color) {
	bool     left = snap->u.loop_item.left;
	FluxRect r
	  = {content->x + (left ? 9.0f : 0.0f), content->y + 3.0f, content->w - (left ? 9.0f : 0.0f), content->h - 9.0f};
	XentTextStyle style = {.font_size = size, .font_weight = 400u, .h_align = left ? 0u : 1u, .v_align = 1u};
	return flux_rec_text_ex(b, &r, snap->u.loop_item.label, &style, color);
}

static bool looping_accent_text(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *content, FluxRect const *bounds,
  FluxRenderSnap const *snap, FluxControlState const *state, float size
) {
	float lo          = snap->u.loop_item.band_y;
	float hi          = lo + snap->u.loop_item.band_h;
	float tops [3]    = {bounds->y, bounds->y + lo, bounds->y + hi};
	float bottoms [3] = {bounds->y + lo, bounds->y + hi, bounds->y + bounds->h};
	for (int i = 0; i < 3; i++) {
		float y0 = fmaxf(tops [i], bounds->y), y1 = fminf(bottoms [i], bounds->y + bounds->h);
		if (y1 <= y0) continue;
		FluxRect  clip  = {bounds->x, y0, bounds->w, y1 - y0};
		FluxColor color = i == 1 ? t->text_on_accent_primary : looping_item_text(t, state);
		if (!xent_dl_pushcliprect(b, flux_rec_xrect(&clip))) return false;
		bool ok = looping_item_label(b, content, snap, size, color);
		if (!xent_dl_popclip(b) || !ok) return false;
	}
	return true;
}

static bool looping_item_fill(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *root, FluxRect const *content, FluxRenderSnap const *snap,
  FluxControlState const *state
) {
	if (state->enabled && state->pressed)
		return flux_rec_fill_rounded(b, root, FLUX_CORNER_RADIUS, t->subtle_fill_tertiary);
	if (state->enabled && state->hovered && state->pointer_type == FLUX_POINTER_MOUSE)
		return flux_rec_fill_rounded(b, root, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);
	if (snap->u.loop_item.selected && !snap->u.loop_item.accent)
		return flux_rec_fill_rounded(b, content, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);
	return true;
}

XentDl *flux_looping_item_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;

	char const *label = snap->u.loop_item.label;
	if (!label) return flux_rec_finish(b, true);

	FluxThemeColors const *t           = flux_rec_theme(rc);
	FluxRect               root        = looping_deflate(bounds, LOOP_ITEM_MARGIN_X, LOOP_ITEM_MARGIN_Y);
	FluxRect               content     = looping_deflate(&root, LOOP_ITEM_CONTENT_X, 0.0f);
	FluxControlState       item_state  = *state;
	item_state.pressed                &= !snap->u.loop_item.selected;
	item_state.hovered                &= !snap->u.loop_item.selected;
	state                              = &item_state;

	if (!looping_item_fill(b, t, &root, &content, snap, state)) return flux_rec_finish(b, false);

	float size = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	if (snap->u.loop_item.accent)
		return flux_rec_finish(b, looping_accent_text(b, t, &content, bounds, snap, state, size));
	return flux_rec_finish(b, looping_item_label(b, &content, snap, size, looping_item_text(t, state)));
}
