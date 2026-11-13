#include "controls/draw/flux_control_record_internal.h"

#include "controls/data/flux_pager_data.h"

#include <stdio.h>

static bool pager_cell_fill(XentDlBuilder *b, FluxRect const *r, bool hover, bool press, FluxThemeColors const *t) {
	FluxColor c = press ? t->subtle_fill_tertiary : hover ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 0);
	if (flux_color_af(c) <= 0.0f) return true;
	return flux_rec_fill_rounded(b, r, 4.0f, c);
}

static bool rec_pager_nav_glyph(
  XentDlBuilder *b, FluxRect const *r, wchar_t const *glyph, bool enabled, bool hover, bool press,
  FluxThemeColors const *t
) {
	if (enabled && !pager_cell_fill(b, r, hover, press, t)) return false;
	return flux_rec_glyph_wc(b, r, glyph, 16.0f, enabled ? t->text_primary : t->text_disabled);
}

static bool pager_draw_text(XentDlBuilder *b, FluxRect const *r, FluxPagerSnapshot const *p, FluxThemeColors const *t) {
	char const *prefix = p->prefix ? p->prefix : "";
	char const *suffix = p->suffix ? p->suffix : "/";
	char        buf [96];
	snprintf(buf, sizeof(buf), "%s%s%d %s %d", prefix, prefix [0] ? " " : "", p->selected + 1, suffix, p->count);
	XentTextStyle style = {.font_size = 14.0f, .font_weight = 400u, .h_align = 1u, .v_align = 1u};
	return flux_rec_text_ex(b, r, buf, &style, t->text_primary);
}

static bool pager_draw_cell(
  XentDlBuilder *b, FluxRect const *r, FluxPagerElem const *e, FluxPagerSnapshot const *p, bool hover, bool press,
  FluxThemeColors const *t
) {
	if (e->page == FLUX_PAGER_ELLIPSIS_PAGE) return flux_rec_glyph_wc(b, r, L"\xE712", 16.0f, t->text_secondary);
	bool selected = e->page == p->selected + 1;
	if (!selected && !pager_cell_fill(b, r, hover, press, t)) return false;
	char buf [8];
	snprintf(buf, sizeof(buf), "%d", e->page);
	XentTextStyle style = {.font_size = 14.0f, .font_weight = selected ? 600u : 400u, .h_align = 1u, .v_align = 1u};
	if (!flux_rec_text_ex(b, r, buf, &style, selected ? t->accent_default : t->text_primary)) return false;
	if (!selected) return true;
	FluxRect ind = {r->x + r->w * 0.5f - 8.0f, r->y + r->h - 3.0f, 16.0f, 2.0f};
	return flux_rec_fill_rounded(b, &ind, 1.0f, t->accent_default);
}

static bool pager_draw_elem(
  XentDlBuilder *b, FluxRect const *r, FluxPagerElem const *e, FluxPagerSnapshot const *p, bool hover, bool press,
  FluxThemeColors const *t
) {
	switch (e->kind) {
	case FLUX_PAGER_EL_FIRST :
		return rec_pager_nav_glyph(b, r, L"\xE892", p->first_state == FLUX_PAGER_NAV_ENABLED, hover, press, t);
	case FLUX_PAGER_EL_PREV :
		return rec_pager_nav_glyph(b, r, L"\xE76B", p->prev_state == FLUX_PAGER_NAV_ENABLED, hover, press, t);
	case FLUX_PAGER_EL_NEXT :
		return rec_pager_nav_glyph(b, r, L"\xE76C", p->next_state == FLUX_PAGER_NAV_ENABLED, hover, press, t);
	case FLUX_PAGER_EL_LAST :
		return rec_pager_nav_glyph(b, r, L"\xE893", p->last_state == FLUX_PAGER_NAV_ENABLED, hover, press, t);
	case FLUX_PAGER_EL_TEXT : return pager_draw_text(b, r, p, t);
	case FLUX_PAGER_EL_CELL : return pager_draw_cell(b, r, e, p, hover, press, t);
	default                 : return true;
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
	  p->display_mode, p->first_state, p->prev_state, p->next_state, p->last_state, p->cells, p->cell_count, elems, &tw
	);

	float hx = (state->hovered && snap->hover_local_x >= 0.0f) ? snap->hover_local_x : -1.0f;
	for (int i = 0; i < n; i++) {
		FluxPagerElem const *e     = &elems [i];
		FluxRect             r     = {bounds->x + e->x, bounds->y, e->w, FLUX_PAGER_HEIGHT};
		bool                 hover = hx >= e->x && hx < e->x + e->w;
		bool                 press = i == p->pressed_elem;
		if (!pager_draw_elem(b, &r, e, p, hover, press, t)) return false;
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

static bool pips_draw_dot(XentDlBuilder *b, FluxRect const *slot, float d, FluxColor color) {
	FluxRect dot = {slot->x + (slot->w - d) * 0.5f, slot->y + (slot->h - d) * 0.5f, d, d};
	return flux_rec_fill_rounded(b, &dot, d * 0.5f, color);
}

static bool pips_draw_caret(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r, wchar_t const *glyph, bool hovered, bool pressed,
  bool enabled
) {
	FluxColor fg = !enabled           ? t->ctrl_strong_fill_disabled
	             : hovered || pressed ? t->text_secondary
	                                  : t->ctrl_strong_fill_default;
	if (pressed) {
		FluxRect s = {r->x + r->w * 0.0625f, r->y + r->h * 0.0625f, r->w * 0.875f, r->h * 0.875f};
		return flux_rec_glyph_wc(b, &s, glyph, 8.0f, fg);
	}
	return flux_rec_glyph_wc(b, r, glyph, 8.0f, fg);
}

bool flux_rec_build_pips(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t        = flux_rec_theme(rc);
	bool                   vertical = snap->u.pips.vertical;
	int                    visible  = snap->u.pips.visible;
	if (visible <= 0) return true;

	bool  nav       = snap->u.pips.nav_vis != 0
	               && (snap->u.pips.nav_vis == 1 || state->hovered || state->focused)
	               && snap->u.pips.count > 1;
	float strip0    = snap->u.pips.nav_vis != 0 ? PIP_NAV : 0.0f;

	bool  hovering  = snap->hover_local_x >= 0.0f;
	float along_hit = vertical ? snap->hover_local_y : snap->hover_local_x;

	for (int i = 0; i < visible; i++) {
		int       index    = snap->u.pips.window_start + i;
		float     along    = strip0 + ( float ) i * PIP_CROSS;
		FluxRect  slot     = pips_slot(vertical, bounds, along, PIP_CROSS);

		bool      over     = hovering && along_hit >= along && along_hit < along + PIP_CROSS;
		bool      selected = index == snap->u.pips.selected;
		bool      pressed  = over && index == snap->u.pips.pressed_pip;
		float     d        = selected || (over && !pressed) ? 6.0f : 4.0f;
		FluxColor fg       = !state->enabled ? t->ctrl_strong_fill_disabled
		                   : over            ? t->text_secondary
		                                     : t->ctrl_strong_fill_default;
		if (!pips_draw_dot(b, &slot, d, fg)) return false;
	}

	if (!nav) return true;

	float    end     = strip0 + ( float ) visible * PIP_CROSS;
	FluxRect prev    = pips_slot(vertical, bounds, 0.0f, PIP_NAV);
	FluxRect next    = pips_slot(vertical, bounds, end, PIP_NAV);

	bool     prev_en = snap->u.pips.selected > 0;
	bool     next_en = snap->u.pips.selected < snap->u.pips.count - 1;
	bool     over_p  = hovering && along_hit < PIP_NAV;
	bool     over_n  = hovering && along_hit >= end && along_hit < end + PIP_NAV;

	if ((prev_en || snap->u.pips.nav_vis == 1)
	    && !pips_draw_caret(
		  b, t, &prev, vertical ? L"\xEDDB" : L"\xEDD9", over_p, over_p && snap->u.pips.pressed_nav == 1, prev_en
		))
		return false;
	if ((next_en || snap->u.pips.nav_vis == 1)
	    && !pips_draw_caret(
		  b, t, &next, vertical ? L"\xEDDC" : L"\xEDDA", over_n, over_n && snap->u.pips.pressed_nav == 2, next_en
		))
		return false;
	return true;
}
