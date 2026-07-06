/**
 * @file flux_pager.c
 * @brief PagerControl renderer: First/Previous/Next/Last nav glyphs around a
 * NumberPanel (numbered cells with an ellipsis "More" glyph and an accent
 * underline under the current page) or a NumberText label. Element geometry
 * comes from flux_pager_layout, the same source hit-testing uses.
 */
#include "controls/draw/flux_button_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "render/flux_render_internal.h"

#include "fluxent/controls/flux_pager_data.h"

#include <stdio.h>

#define PAGER_NAV_FIRST L"\xE892"
#define PAGER_NAV_PREV  L"\xE76B"
#define PAGER_NAV_NEXT  L"\xE76C"
#define PAGER_NAV_LAST  L"\xE893"
#define PAGER_ELLIPSIS  L"\xE712"

static void pager_cell_fill(FluxRenderContext const *rc, FluxRect const *r, bool hover, bool press, FluxThemeColors const *t) {
	FluxColor c = press ? t->subtle_fill_tertiary : hover ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 0);
	if (flux_color_af(c) > 0.0f) flux_fill_rounded_rect(rc, r, 4.0f, c);
}

static void pager_draw_number_text(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *r, FluxThemeColors const *t
) {
	if (!rc->text) return;
	char const *prefix = snap->u.pager.prefix ? snap->u.pager.prefix : "";
	char const *suffix = snap->u.pager.suffix ? snap->u.pager.suffix : "/";
	char        buf [96];
	snprintf(buf, sizeof(buf), "%s%s%d %s %d", prefix, prefix [0] ? " " : "", snap->u.pager.selected + 1, suffix, snap->u.pager.count);

	FluxTextStyle ts = {0};
	ts.font_size  = 14.0f;
	ts.text_align = FLUX_TEXT_CENTER;
	ts.vert_align = FLUX_TEXT_VCENTER;
	ts.color      = t->text_primary;
	flux_text_draw(rc->text, FLUX_RT(rc), buf, r, &ts);
}

/* First / Previous / Next / Last nav glyph: greyed and inert when its state is
 * disabled, subtle-fill chrome when enabled. */
static void pager_draw_nav(
  FluxRenderContext const *rc, FluxRect const *r, int kind, FluxPagerSnapshot const *p, bool hover, bool press,
  FluxThemeColors const *t
) {
	int st = kind == FLUX_PAGER_EL_FIRST ? p->first_state
	       : kind == FLUX_PAGER_EL_PREV  ? p->prev_state
	       : kind == FLUX_PAGER_EL_NEXT  ? p->next_state
	                                     : p->last_state;
	wchar_t const *g = kind == FLUX_PAGER_EL_FIRST ? PAGER_NAV_FIRST
	                 : kind == FLUX_PAGER_EL_PREV  ? PAGER_NAV_PREV
	                 : kind == FLUX_PAGER_EL_NEXT  ? PAGER_NAV_NEXT
	                                               : PAGER_NAV_LAST;
	bool enabled = st == FLUX_PAGER_NAV_ENABLED;
	if (enabled) pager_cell_fill(rc, r, hover, press, t);
	flux_button_draw_chevron(rc, r, g, 16.0f, enabled ? t->text_primary : t->text_disabled);
}

/* A NumberPanel cell: the ellipsis "More" glyph, or a page number carrying an
 * accent weight + underline when it is the current page. */
static void pager_draw_cell(
  FluxRenderContext const *rc, FluxRect const *r, FluxPagerElem const *e, FluxPagerSnapshot const *p, bool hover,
  bool press, FluxThemeColors const *t
) {
	if (e->page == FLUX_PAGER_ELLIPSIS_PAGE) {
		flux_button_draw_chevron(rc, r, PAGER_ELLIPSIS, 16.0f, t->text_secondary);
		return;
	}
	bool selected = e->page == p->selected + 1;
	if (!selected) pager_cell_fill(rc, r, hover, press, t);

	char buf [8];
	snprintf(buf, sizeof(buf), "%d", e->page);
	FluxTextStyle ts = {0};
	ts.font_size   = 14.0f;
	ts.font_weight = selected ? FLUX_FONT_SEMI_BOLD : FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_CENTER;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = selected ? t->accent_default : t->text_primary;
	if (rc->text) flux_text_draw(rc->text, FLUX_RT(rc), buf, r, &ts);

	if (!selected) return;
	FluxRect ind = {r->x + r->w * 0.5f - 8.0f, r->y + r->h - 3.0f, 16.0f, 2.0f};
	flux_fill_rounded_rect(rc, &ind, 1.0f, t->accent_default);
}

void flux_draw_pager(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const   *t = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxPagerSnapshot const *p = &snap->u.pager;

	int16_t const *cells = p->cells;
	FluxPagerElem  elems [FLUX_PAGER_MAX_CELLS + 4];
	float          tw = 0.0f;
	int            n  = flux_pager_layout(
	   p->display_mode, p->first_state, p->prev_state, p->next_state, p->last_state, cells, p->cell_count, elems, &tw
	  );

	float hx = (state->hovered && snap->hover_local_x >= 0.0f) ? snap->hover_local_x : -1.0f;

	for (int i = 0; i < n; i++) {
		FluxPagerElem const *e     = &elems [i];
		FluxRect             r     = {bounds->x + e->x, bounds->y, e->w, FLUX_PAGER_HEIGHT};
		bool                 hover = hx >= e->x && hx < e->x + e->w;
		bool                 press = i == p->pressed_elem;

		switch (e->kind) {
		case FLUX_PAGER_EL_FIRST:
		case FLUX_PAGER_EL_PREV:
		case FLUX_PAGER_EL_NEXT:
		case FLUX_PAGER_EL_LAST: pager_draw_nav(rc, &r, e->kind, p, hover, press, t); break;
		case FLUX_PAGER_EL_TEXT: pager_draw_number_text(rc, snap, &r, t); break;
		case FLUX_PAGER_EL_CELL: pager_draw_cell(rc, &r, e, p, hover, press, t); break;
		default: break;
		}
	}
}
