#include "controls/draw/flux_control_record_internal.h"

#include "controls/data/flux_pager_data.h"

#include <stdio.h>

typedef struct PagerInteraction {
	bool enabled, hovered, pressed;
} PagerInteraction;

static FluxColor pager_fg(FluxThemeColors const *t, PagerInteraction s) {
	if (!t->contrast.active) return s.enabled ? t->text_primary : t->text_disabled;
	if (!s.enabled) return t->contrast.gray_text;
	return s.hovered || s.pressed ? t->contrast.highlight_text : t->contrast.button_text;
}

static bool pager_cell_fill(XentDlBuilder *b, FluxRect const *r, FluxThemeColors const *t, PagerInteraction s) {
	if (!s.enabled || (!s.hovered && !s.pressed)) return true;
	FluxColor c = s.pressed ? t->subtle_fill_tertiary : t->subtle_fill_secondary;
	if (t->contrast.active) c = t->contrast.highlight;
	return flux_rec_fill_rounded(b, r, FLUX_CORNER_RADIUS, c);
}

static bool rec_pager_nav_glyph(
  XentDlBuilder *b, FluxRect const *r, wchar_t const *glyph, uint8_t state, PagerInteraction s, FluxThemeColors const *t
) {
	if (state == FLUX_PAGER_NAV_INVISIBLE) return true;
	s.enabled = s.enabled && state == FLUX_PAGER_NAV_ENABLED;
	return pager_cell_fill(b, r, t, s) && flux_rec_arrow_wc(b, r, glyph, 16.0f, pager_fg(t, s));
}

static bool pager_draw_cell(
  XentDlBuilder *b, FluxRect const *r, FluxPagerElem const *e, FluxPagerSnapshot const *p, PagerInteraction s,
  FluxThemeColors const *t
) {
	if (e->page == FLUX_PAGER_ELLIPSIS_PAGE) {
		FluxColor ink = t->contrast.active ? t->contrast.button_text : t->text_secondary;
		return flux_rec_glyph_wc(b, r, L"\xE712", 16.0f, ink);
	}
	bool selected = e->page == p->selected + 1;
	char buf [16];
	snprintf(buf, sizeof(buf), "%d", e->page);
	XentTextStyle style = {.font_size = 14.0f, .font_weight = FLUX_FONT_REGULAR, .h_align = 1u, .v_align = 1u};
	bool          ok    = pager_cell_fill(b, r, t, s) && flux_rec_text_ex(b, r, buf, &style, pager_fg(t, s));
	if (!ok || !selected) return ok;
	FluxRect  ind = {r->x, r->y + r->h - 2.0f, r->w, 2.0f};
	FluxColor ink = t->contrast.active ? t->contrast.button_text : t->accent_default;
	return flux_rec_fill_rounded(b, &ind, 1.0f, ink);
}

static bool pager_draw_elem(
  XentDlBuilder *b, FluxRect const *r, FluxPagerElem const *e, FluxPagerSnapshot const *p, PagerInteraction s,
  FluxThemeColors const *t
) {
	switch (e->kind) {
	case FLUX_PAGER_EL_FIRST : return rec_pager_nav_glyph(b, r, L"\xE892", p->first_state, s, t);
	case FLUX_PAGER_EL_PREV  : return rec_pager_nav_glyph(b, r, L"\xE76B", p->prev_state, s, t);
	case FLUX_PAGER_EL_NEXT  : return rec_pager_nav_glyph(b, r, L"\xE76C", p->next_state, s, t);
	case FLUX_PAGER_EL_LAST  : return rec_pager_nav_glyph(b, r, L"\xE893", p->last_state, s, t);
	case FLUX_PAGER_EL_CELL  : return pager_draw_cell(b, r, e, p, s, t);
	default                  : return true;
	}
}

bool flux_rec_build_pager(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const   *t = flux_rec_theme(rc);
	FluxPagerSnapshot const *p = &snap->u.pager;
	FluxPagerElem            elems [FLUX_PAGER_MAX_CELLS + 4];
	float                    tw = 0.0f;
	int                      n  = flux_pager_layout(
	  p->display_mode, p->first_state, p->prev_state, p->next_state, p->last_state, p->cells, p->cell_count, p->box_w,
	  elems, &tw, rc ? rc->text : NULL
	);

	float hx = (state->hovered && snap->hover_local_x >= 0.0f) ? snap->hover_local_x : -1.0f;
	for (int i = 0; i < n; i++) {
		FluxPagerElem const *e = &elems [i];
		FluxRect             r = {bounds->x + e->x, bounds->y, e->w, FLUX_PAGER_HEIGHT};
		PagerInteraction     s = {state->enabled != 0, hx >= e->x && hx < e->x + e->w, i == p->pressed_elem};
		if (!pager_draw_elem(b, &r, e, p, s, t)) return false;
		bool focus = e->kind == p->focus_kind && (e->kind != FLUX_PAGER_EL_CELL || e->page == p->focus_page);
		if (s.enabled && focus && !flux_rec_focus_rect(b, t, &r, FLUX_CORNER_RADIUS)) return false;
	}
	return true;
}

#define PIP_MAIN  24.0f
#define PIP_CROSS 12.0f
#define PIP_NAV   24.0f

static FluxRect pips_slot(bool vertical, FluxRect const *b, float along, float along_len) {
	return vertical ? (FluxRect) {b->x, b->y + along, PIP_MAIN, along_len}
	                : (FluxRect) {b->x + along, b->y, along_len, PIP_MAIN};
}

static FluxColor pips_ink(FluxThemeColors const *t, PagerInteraction s, bool selected) {
	if (!t->contrast.active) {
		if (!s.enabled) return t->ctrl_strong_fill_disabled;
		return s.hovered || s.pressed ? t->text_secondary : t->ctrl_strong_fill_default;
	}
	if (!s.enabled) return t->contrast.gray_text;
	return selected || s.hovered || s.pressed ? t->contrast.highlight_text : t->contrast.button_text;
}

static bool
pips_draw_dot(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *slot, PagerInteraction s, bool selected) {
	bool hot = s.enabled && (selected || s.hovered || s.pressed);
	if (t->contrast.active) {
		FluxColor fill = hot ? t->contrast.highlight : t->contrast.button_face;
		if (!flux_rec_fill_rounded(b, slot, FLUX_CORNER_RADIUS, fill)) return false;
	}
	float size = !s.pressed && (selected || s.hovered) ? 6.0f : 4.0f;
	return flux_rec_glyph_wc(b, slot, L"\xEA3B", size, pips_ink(t, s, selected));
}

static bool pips_draw_caret(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r, wchar_t const *glyph, PagerInteraction s
) {
	float size = s.enabled && s.pressed ? 7.0f : 8.0f;
	if (!t->contrast.active) return flux_rec_arrow_wc(b, r, glyph, size, pips_ink(t, s, false));
	FluxContrastColors const *p      = &t->contrast;
	bool                      hot    = s.enabled && (s.hovered || s.pressed);
	FluxColor                 stroke = hot ? p->highlight : p->button_face;
	FluxColor                 fg     = s.enabled ? (hot ? p->highlight : p->button_text) : p->gray_text;
	return flux_rec_fill_rounded(b, r, FLUX_CORNER_RADIUS, p->button_face)
	    && flux_rec_stroke_rounded(b, r, FLUX_CORNER_RADIUS - 0.5f, stroke, 1.0f)
	    && flux_rec_arrow_wc(b, r, glyph, size, fg);
}

static bool pips_draw_strip(
  XentDlBuilder *b, FluxThemeColors const *t, FluxPipsSnapshot const *p, FluxRect const *bounds,
  FluxControlState const *state, float hit
) {
	float start = p->nav_vis != 0 ? PIP_NAV : 0.0f;
	for (int i = 0; i < p->visible; i++) {
		int              index    = p->window_start + i;
		float            along    = start + ( float ) i * PIP_CROSS;
		FluxRect         slot     = pips_slot(p->vertical, bounds, along, PIP_CROSS);
		bool             over     = state->enabled && hit >= along && hit < along + PIP_CROSS;
		PagerInteraction s        = {state->enabled != 0, over, over && index == p->pressed_pip};
		bool             selected = index == p->selected;
		if (!pips_draw_dot(b, t, &slot, s, selected)) return false;
	}
	return true;
}

static bool pips_draw_nav(
  XentDlBuilder *b, FluxThemeColors const *t, FluxPipsSnapshot const *p, FluxRect const *bounds, bool enabled, float hit
) {
	float            end     = PIP_NAV + ( float ) p->visible * PIP_CROSS;
	FluxRect         prev    = pips_slot(p->vertical, bounds, 0.0f, PIP_NAV);
	FluxRect         next    = pips_slot(p->vertical, bounds, end, PIP_NAV);
	bool             prev_en = p->selected > 0;
	bool             next_en = p->selected < p->count - 1;
	bool             over_p  = hit >= 0.0f && hit < PIP_NAV;
	bool             over_n  = hit >= end && hit < end + PIP_NAV;
	PagerInteraction ps      = {enabled && prev_en, over_p, over_p && p->pressed_nav == 1};
	PagerInteraction ns      = {enabled && next_en, over_n, over_n && p->pressed_nav == 2};

	if ((prev_en || p->nav_vis == 1) && !pips_draw_caret(b, t, &prev, p->vertical ? L"\xEDDB" : L"\xEDD9", ps))
		return false;
	return (!next_en && p->nav_vis != 1) || pips_draw_caret(b, t, &next, p->vertical ? L"\xEDDC" : L"\xEDDA", ns);
}

static bool
pips_draw_focus(XentDlBuilder *b, FluxThemeColors const *t, FluxPipsSnapshot const *p, FluxRect const *bounds) {
	int index = p->selected - p->window_start;
	if (index < 0 || index >= p->visible) return true;
	float    start = p->nav_vis != 0 ? PIP_NAV : 0.0f;
	FluxRect slot  = pips_slot(p->vertical, bounds, start + ( float ) index * PIP_CROSS, PIP_CROSS);
	return flux_rec_focus_rect(b, t, &slot, FLUX_CORNER_RADIUS);
}

bool flux_rec_build_pips(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxPipsSnapshot const *p = &snap->u.pips;
	if (p->visible <= 0) return true;
	FluxThemeColors const *t        = flux_rec_theme(rc);
	bool                   hovering = snap->hover_local_x >= 0.0f;
	float                  hit      = p->vertical && hovering ? snap->hover_local_y : snap->hover_local_x;
	if (!pips_draw_strip(b, t, p, bounds, state, hit)) return false;
	bool nav = p->nav_vis != 0 && p->count > 1 && (p->nav_vis == 1 || state->hovered || state->focused);
	bool ok  = !nav || pips_draw_nav(b, t, p, bounds, state->enabled != 0, hit);
	return ok && (!state->enabled || !state->focused || pips_draw_focus(b, t, p, bounds));
}
