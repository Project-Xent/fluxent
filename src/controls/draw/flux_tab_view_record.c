#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_control_draw.h"

#include "controls/data/flux_tab_view_data.h"

bool flux_rec_tabs(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds) {
	FluxRect               sb      = flux_rec_snap_bounds(rc, bounds);
	FluxThemeColors const *t       = flux_rec_theme(rc);
	float                  seam    = sb.y + FLUX_TAB_STRIP_H;
	FluxRect               content = {sb.x, seam, sb.w, flux_maxf(0.0f, sb.h - FLUX_TAB_STRIP_H)};
	bool                   ok      = flux_rec_fill_rect(b, &content, t->solid_background_tertiary);
	if (t->contrast.active) {
		FluxRect strip = {sb.x, sb.y, sb.w, FLUX_TAB_STRIP_H};
		ok             = ok && flux_rec_fill_rect(b, &strip, t->contrast.button_face);
	}
	FluxColor line = t->contrast.active ? t->contrast.highlight : t->card_stroke_default;
	return ok && flux_rec_linehs(b, rc, sb.x, sb.x + sb.w, seam - 0.5f, 1.0f, line);
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

static bool tab_rec_aux_glyph(XentDlBuilder *b, FluxRect const *box, uint8_t kind, FluxColor fg) {
	if (kind == FLUX_TAB_KIND_ADD) return flux_rec_glyph_wc(b, box, L"\xE710", FLUX_TAB_CLOSE_FONT, fg);
	wchar_t const *g = kind == FLUX_TAB_KIND_SCROLL_DEC ? L"\xEDD9" : L"\xEDDA";
	return flux_rec_arrow_wc(b, box, g, FLUX_TAB_SCROLL_FONT, fg);
}

static FluxRecButtonColors
tab_close_contrast(FluxContrastColors const *p, FluxControlState const *state, FluxTabSnapshot const *s) {
	if (!state->enabled || s->tab_owner_disabled)
		return (FluxRecButtonColors) {.fill = p->window, .text = p->gray_text};
	if (state->pressed || state->hovered)
		return (FluxRecButtonColors) {.fill = p->button_face, .stroke = p->button_text, .text = p->button_text};
	if (s->is_checked) return (FluxRecButtonColors) {.text = p->highlight};
	if (s->tab_owner_hot) return (FluxRecButtonColors) {.text = p->highlight_text};
	return (FluxRecButtonColors) {.fill = p->button_face, .text = p->button_text};
}

static FluxRecButtonColors tab_aux_contrast(FluxContrastColors const *p, FluxControlState const *state) {
	if (!state->enabled) return (FluxRecButtonColors) {.fill = p->window, .stroke = p->gray_text, .text = p->gray_text};
	if (state->hovered || state->pressed)
		return (FluxRecButtonColors) {.fill = p->highlight, .stroke = p->highlight, .text = p->highlight_text};
	return (FluxRecButtonColors) {.fill = p->button_face, .stroke = p->button_text, .text = p->button_text};
}

static bool tab_rec_aux_contrast(
  XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, FluxContrastColors const *p,
  FluxTabSnapshot const *s
) {
	bool                close = s->tab_kind == FLUX_TAB_KIND_CLOSE;
	FluxRecButtonColors c     = close ? tab_close_contrast(p, state, s) : tab_aux_contrast(p, state);
	bool                ok    = flux_rec_fill_rounded(b, box, FLUX_CORNER_RADIUS, c.fill)
	                         && flux_rec_stroke_rounded(b, box, FLUX_CORNER_RADIUS - 0.5f, c.stroke, 1.0f);
	if (!ok) return false;
	if (close) return flux_rec_glyph_wc(b, box, L"\xE711", FLUX_TAB_CLOSE_FONT, c.text);
	return tab_rec_aux_glyph(b, box, s->tab_kind, c.text);
}

static bool tab_rec_aux(
  XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, FluxThemeColors const *t,
  FluxTabSnapshot const *s
) {
	if (t->contrast.active) return tab_rec_aux_contrast(b, box, state, &t->contrast, s);
	if (s->tab_kind == FLUX_TAB_KIND_CLOSE) return tab_rec_close(b, box, state, t);
	if (!tab_rec_button_box(b, box, state, t)) return false;
	FluxColor normal = s->tab_kind == FLUX_TAB_KIND_ADD && !state->pressed ? t->text_primary : t->text_secondary;
	return tab_rec_aux_glyph(b, box, s->tab_kind, state->enabled ? normal : t->text_disabled);
}

static bool tab_rec_selected_outline(XentDlBuilder *b, FluxRect const *box, FluxColor color) {
	FluxRect clip = {box->x - 1.0f, box->y, box->w + 2.0f, box->h - FLUX_TAB_SELECTED_FLARE};
	FluxRect ext  = {clip.x, clip.y, clip.w, clip.h + FLUX_TAB_CORNER};
	if (!xent_dl_pushcliprect(b, flux_rec_xrect(&clip))) return false;
	bool ok = flux_rec_stroke_rounded(b, &ext, FLUX_TAB_CORNER - 0.5f, color, 1.0f);
	return xent_dl_popclip(b) && ok;
}

static bool
tab_rec_selected_contrast(XentDlBuilder *b, FluxRect const *box, FluxContrastColors const *p, bool enabled) {
	FluxRect  body = {box->x - 1.0f, box->y, box->w + 2.0f, box->h - 1.0f};
	FluxColor fill = enabled ? p->button_face : p->window;
	return flux_rec_fill_shape(b, FLUX_SHAPE_TAB_SELECTED_FRAME, box, p->window)
	    && flux_rec_filltop(b, &body, FLUX_TAB_CORNER, fill)
	    && tab_rec_selected_outline(b, box, p->highlight)
	    && flux_rec_fill_shape(b, FLUX_SHAPE_TAB_SELECTED_ARCS, box, p->highlight);
}

static bool tab_rec_item_contrast(
  XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, FluxContrastColors const *p,
  FluxTabSnapshot const *s
) {
	if (s->tab_dragging)
		return flux_rec_fill_rounded(b, box, FLUX_TAB_CORNER, p->window)
		    && flux_rec_stroke_rounded(b, box, FLUX_TAB_CORNER - 0.5f, p->highlight, 1.0f);
	if (s->is_checked) return tab_rec_selected_contrast(b, box, p, state->enabled != 0);
	FluxRect body = {box->x, box->y, box->w, box->h - 1.0f};
	if (!state->enabled) return flux_rec_filltop(b, &body, FLUX_TAB_CORNER, p->window);
	if (state->hovered || state->pressed) return flux_rec_filltop(b, &body, FLUX_TAB_CORNER, p->highlight);
	return true;
}

static bool tab_rec_item_fill(
  XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, FluxTabSnapshot const *s,
  FluxThemeColors const *t
) {
	if (t->contrast.active) return tab_rec_item_contrast(b, box, state, &t->contrast, s);
	if (s->is_checked) return flux_rec_fill_shape(b, FLUX_SHAPE_TAB_SELECTED_FRAME, box, t->solid_background_tertiary);
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

static FluxColor tab_item_contrast_fg(FluxContrastColors const *p, FluxControlState const *state, bool selected) {
	if (!state->enabled) return p->gray_text;
	if (selected) return p->highlight;
	return state->hovered || state->pressed ? p->highlight_text : p->window_text;
}

static FluxColor tab_item_fg(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (t->contrast.active) return tab_item_contrast_fg(&t->contrast, state, selected);
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
	  t->contrast.active ? t->contrast.window_text : t->divider_stroke_default
	);
}

static bool tab_rec_header(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t        = flux_rec_theme(rc);
	bool                   selected = snap->u.tab.is_checked;
	if (!selected && !xent_dl_pushcliprect(b, flux_rec_xrect(bounds))) return false;
	bool ok = tab_rec_item_fill(b, bounds, state, &snap->u.tab, t);
	ok      = ok && tab_rec_icon_label(b, bounds, snap, tab_item_fg(t, state, selected), selected);
	ok      = ok && tab_item_separator(b, bounds, t, snap->u.tab.tab_separator);
	if (!selected) ok = xent_dl_popclip(b) && ok;
	return ok;
}

bool flux_rec_tabitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (bounds->w < 1.0f || bounds->h < 1.0f) return true;
	FluxThemeColors const *t      = flux_rec_theme(rc);
	bool                   header = snap->u.tab.tab_kind == FLUX_TAB_KIND_TAB;
	bool ok = header ? tab_rec_header(b, rc, snap, bounds, state) : tab_rec_aux(b, bounds, state, t, &snap->u.tab);
	if (ok && state->focused && state->enabled)
		ok = flux_rec_focus_rect(b, t, bounds, header ? FLUX_TAB_CORNER : FLUX_CORNER_RADIUS);
	return ok;
}
