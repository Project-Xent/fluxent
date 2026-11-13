#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_control_draw.h"

#include "controls/data/flux_tab_view_data.h"

bool flux_rec_tabs(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds) {
	FluxRect               sb      = flux_rec_snap_bounds(rc, bounds);
	FluxThemeColors const *t       = flux_rec_theme(rc);
	float                  seam    = sb.y + FLUX_TAB_STRIP_H;
	FluxRect               content = {sb.x, seam, sb.w, flux_maxf(0.0f, sb.h - FLUX_TAB_STRIP_H)};
	bool                   ok      = flux_rec_fill_rect(b, &content, t->solid_background_tertiary);
	if (ok) ok = flux_rec_linehs(b, rc, sb.x, sb.x + sb.w, seam - 0.5f, 1.0f, t->card_stroke_default);
	return ok;
}

static bool
tab_rec_button_box(XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, FluxThemeColors const *t) {
	if (state->enabled && state->pressed)
		return flux_rec_fill_rounded(b, box, FLUX_CORNER_RADIUS, t->subtle_fill_tertiary);
	if (state->enabled && state->hovered)
		return flux_rec_fill_rounded(b, box, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);
	return true;
}

static bool
tab_rec_close(XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, FluxThemeColors const *t) {
	FluxColor fill = state->pressed ? t->subtle_fill_tertiary : t->subtle_fill_secondary;
	if ((state->pressed || state->hovered) && !flux_rec_fill_rounded(b, box, FLUX_CORNER_RADIUS, fill)) return false;
	return flux_rec_glyph_wc(
	  b, box, L"\xE711", FLUX_TAB_CLOSE_FONT, state->pressed ? t->text_secondary : t->text_primary
	);
}

static bool tab_rec_aux(
  XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, FluxThemeColors const *t, uint8_t kind
) {
	if (kind == FLUX_TAB_KIND_CLOSE) return tab_rec_close(b, box, state, t);
	if (!tab_rec_button_box(b, box, state, t)) return false;
	if (kind == FLUX_TAB_KIND_ADD) {
		FluxColor fg = !state->enabled ? t->text_disabled : (state->pressed ? t->text_secondary : t->text_primary);
		return flux_rec_glyph_wc(b, box, L"\xE710", FLUX_TAB_CLOSE_FONT, fg);
	}
	FluxColor      fg = state->enabled ? t->text_secondary : t->text_disabled;
	wchar_t const *g  = kind == FLUX_TAB_KIND_SCROLL_DEC ? L"\xEDD9" : L"\xEDDA";
	return flux_rec_glyph_wc(b, box, g, FLUX_TAB_SCROLL_FONT, fg);
}

static bool tab_rec_item_fill(
  XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, bool selected, FluxThemeColors const *t
) {
	if (selected) return flux_rec_fill_shape(b, FLUX_SHAPE_TAB_SELECTED_FRAME, box, t->solid_background_tertiary);
	if (state->enabled && state->pressed)
		return flux_rec_filltop(b, box, FLUX_TAB_CORNER, t->layer_on_mica_alt_default);
	if (state->enabled && state->hovered)
		return flux_rec_filltop(b, box, FLUX_TAB_CORNER, t->layer_on_mica_alt_secondary);
	return true;
}

static bool
tab_rec_icon_label(XentDlBuilder *b, FluxRect const *box, FluxRenderSnap const *snap, FluxColor fg, bool selected) {
	float          x     = box->x + FLUX_TAB_PAD_L;
	wchar_t const *glyph = flux_icon_lookup(snap->u.tab.icon_name);
	bool           ok    = true;
	if (glyph) {
		FluxRect ib  = {x, box->y, FLUX_TAB_ICON, box->h};
		ok           = flux_rec_glyph_wc(b, &ib, glyph, FLUX_TAB_ICON, fg);
		x           += FLUX_TAB_ICON + FLUX_TAB_ICON_GAP;
	}
	float close_w
	  = snap->u.tab.tab_closable ? FLUX_TAB_CLOSE_MARGIN_L + FLUX_TAB_CLOSE_W + FLUX_TAB_PAD_R : FLUX_TAB_PAD_R_NOCLOSE;
	float text_right = box->x + box->w - close_w;
	if (!ok || !snap->u.tab.label || text_right <= x) return ok;
	FluxRect tr     = {x, box->y, text_right - x, box->h};
	uint16_t weight = selected ? ( uint16_t ) FLUX_FONT_SEMI_BOLD : ( uint16_t ) FLUX_FONT_REGULAR;
	ok              = xent_dl_pushcliprect(b, flux_rec_xrect(&tr));
	if (ok) ok = flux_rec_text(b, &tr, snap->u.tab.label, FLUX_TAB_FONT, weight, fg);
	return xent_dl_popclip(b) && ok;
}

static FluxColor tab_item_fg(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (!state->enabled) return t->text_disabled;
	if (state->pressed && !selected) return t->text_tertiary;
	if (!selected && !state->hovered && !state->pressed) return t->text_secondary;
	return t->text_primary;
}

static bool tab_item_separator(XentDlBuilder *b, FluxRect const *box, FluxThemeColors const *t, bool show) {
	if (!show) return true;
	float x = box->x + box->w - FLUX_STROKE_WIDTH * 0.5f;
	return flux_rec_strokev(
	  b, x, box->y + FLUX_TAB_SEP_MARGIN_V, box->y + box->h - FLUX_TAB_SEP_MARGIN_V, FLUX_STROKE_WIDTH,
	  t->divider_stroke_default
	);
}

bool flux_rec_tabitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t   = flux_rec_theme(rc);
	FluxRect               box = *bounds;
	if (box.w < 1.0f || box.h < 1.0f) return true;
	if (snap->u.tab.tab_kind != FLUX_TAB_KIND_TAB) return tab_rec_aux(b, &box, state, t, snap->u.tab.tab_kind);

	bool selected = snap->u.tab.is_checked;
	if (!selected && !xent_dl_pushcliprect(b, flux_rec_xrect(&box))) return false;
	bool ok = tab_rec_item_fill(b, &box, state, selected, t);
	if (ok) ok = tab_rec_icon_label(b, &box, snap, tab_item_fg(t, state, selected), selected);
	if (ok) ok = tab_item_separator(b, &box, t, snap->u.tab.tab_separator);
	if (!selected) ok = xent_dl_popclip(b) && ok;
	return ok;
}
