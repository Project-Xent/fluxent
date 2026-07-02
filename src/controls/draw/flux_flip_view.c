/**
 * @file flux_flip_view.c
 * @brief FlipView + PipsPager renderers, 1:1 with the WinUI 3 templates
 * (controls/dev/CommonStyles/FlipView_themeresources.xaml):
 *
 * - FlipView: SolidBackgroundFillColorBase surface; 16×38 rounded acrylic
 *   nav chips with a 1px edge margin (mirrored for vertical orientation),
 *   CaretSolid8 arrows (EDD9/EDDA; EDDB/EDDC vertical) at 8px — strong
 *   fill, secondary-text on hover/press (pressed shrinks the caret to
 *   0.875 like PipsPager). Shown while the pointer is active (3 s fade)
 *   or focused, hidden on the first/last page edge.
 * - PipsPager: U+EA3B dots — 4px normal / 6px selected-or-hover, strong
 *   fill (secondary text on hover/press), CaretSolid8 nav carets at 8px in
 *   24×24 targets, pressed carets at 0.875 scale.
 */
#include "controls/draw/flux_button_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_render_internal.h"

#include <math.h>
#include <windows.h>

#define FLIP_BTN_W      16.0f
#define FLIP_BTN_H      38.0f
#define FLIP_BTN_MARGIN 1.0f
#define PIP_MAIN    24.0f
#define PIP_CROSS   12.0f
#define PIP_NAV     24.0f

void flux_draw_flip_view(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxThemeColors const *t = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxColor bg             = snap->background.rgba ? snap->background : t->flip_view_bg;
	flux_fill_rect(rc, bounds, bg);
}

/* WinUI 3 nav chip: acrylic fill (all states), rounded ControlCornerRadius,
 * CaretSolid8 arrow — strong fill, secondary text on hover/press, pressed
 * shrinks the caret about center (same family as PipsPager). */
static void flip_draw_button(
  FluxRenderContext const *rc, FluxThemeColors const *t, FluxRect const *r, wchar_t const *glyph, bool hovered,
  bool pressed
) {
	flux_fill_rounded_rect(rc, r, FLUX_CORNER_RADIUS, t->flip_nav_bg_default);
	FluxColor fg = hovered || pressed ? t->text_secondary : t->ctrl_strong_fill_default;
	if (pressed) {
		FluxRect s = {r->x + r->w * 0.0625f, r->y + r->h * 0.0625f, r->w * 0.875f, r->h * 0.875f};
		flux_button_draw_chevron(rc, &s, glyph, 8.0f, fg);
	}
	else flux_button_draw_chevron(rc, r, glyph, 8.0f, fg);
}

void flux_draw_flip_view_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t = rc->theme ? rc->theme : flux_theme_default_colors();

	bool vertical  = snap->u.flip.vertical;
	bool hovered   = snap->hover_local_x >= 0.0f;
	bool alive     = snap->u.flip.buttons_alive;
	if (!alive && !hovered) return;

	float hx = snap->hover_local_x, hy = snap->hover_local_y;

	if (snap->u.flip.prev_enabled) {
		FluxRect r = vertical
		    ? (FluxRect) {bounds->x + (bounds->w - FLIP_BTN_H) * 0.5f, bounds->y + FLIP_BTN_MARGIN, FLIP_BTN_H,
		                  FLIP_BTN_W}
		    : (FluxRect) {bounds->x + FLIP_BTN_MARGIN, bounds->y + (bounds->h - FLIP_BTN_H) * 0.5f, FLIP_BTN_W,
		                  FLIP_BTN_H};
		bool over = hovered && hx >= r.x - bounds->x && hx < r.x - bounds->x + r.w && hy >= r.y - bounds->y
		         && hy < r.y - bounds->y + r.h;
		flip_draw_button(rc, t, &r, vertical ? L"\xEDDB" : L"\xEDD9", over, over && snap->u.flip.pressed_btn == 1);
	}
	if (snap->u.flip.next_enabled) {
		FluxRect r = vertical
		    ? (FluxRect) {bounds->x + (bounds->w - FLIP_BTN_H) * 0.5f,
		                  bounds->y + bounds->h - FLIP_BTN_W - FLIP_BTN_MARGIN, FLIP_BTN_H, FLIP_BTN_W}
		    : (FluxRect) {bounds->x + bounds->w - FLIP_BTN_W - FLIP_BTN_MARGIN,
		                  bounds->y + (bounds->h - FLIP_BTN_H) * 0.5f, FLIP_BTN_W, FLIP_BTN_H};
		bool over = hovered && hx >= r.x - bounds->x && hx < r.x - bounds->x + r.w && hy >= r.y - bounds->y
		         && hy < r.y - bounds->y + r.h;
		flip_draw_button(rc, t, &r, vertical ? L"\xEDDC" : L"\xEDDA", over, over && snap->u.flip.pressed_btn == 2);
	}
}

/* --------------------------------------------------------------------- */

static void pips_draw_dot(FluxRenderContext const *rc, FluxRect const *slot, float d, FluxColor color) {
	FluxRect dot = {slot->x + (slot->w - d) * 0.5f, slot->y + (slot->h - d) * 0.5f, d, d};
	flux_fill_rounded_rect(rc, &dot, d * 0.5f, color);
}

static void pips_draw_caret(
  FluxRenderContext const *rc, FluxThemeColors const *t, FluxRect const *r, wchar_t const *glyph, bool hovered,
  bool pressed, bool enabled
) {
	FluxColor fg = !enabled ? t->ctrl_strong_fill_disabled
	    : hovered || pressed ? t->text_secondary
	                         : t->ctrl_strong_fill_default;
	if (pressed) {
		/* PipsPagerNavigationButtonScalePressed 0.875 — shrink about center. */
		FluxRect s = {r->x + r->w * 0.0625f, r->y + r->h * 0.0625f, r->w * 0.875f, r->h * 0.875f};
		flux_button_draw_chevron(rc, &s, glyph, 8.0f, fg);
	}
	else flux_button_draw_chevron(rc, r, glyph, 8.0f, fg);
}

/* The pager stacks pips (and the two nav carets) along one axis; `vertical`
 * only decides which screen axis that is. Map an (along, along_len) span onto
 * a screen rect (the cross extent is always PIP_MAIN) so the layout math below
 * is written once, in along-space, with no per-line orientation branch. */
static FluxRect pips_slot(bool vertical, FluxRect const *b, float along, float along_len) {
	return vertical ? (FluxRect) {b->x, b->y + along, PIP_MAIN, along_len}
	                : (FluxRect) {b->x + along, b->y, along_len, PIP_MAIN};
}

void flux_draw_pips_pager(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t = rc->theme ? rc->theme : flux_theme_default_colors();

	bool  vertical = snap->u.pips.vertical;
	int   visible  = snap->u.pips.visible;
	bool  nav      = snap->u.pips.nav_vis != 0
	         && (snap->u.pips.nav_vis == 1 || state->hovered || state->focused) && snap->u.pips.count > 1;
	float strip0   = snap->u.pips.nav_vis != 0 ? PIP_NAV : 0.0f;

	bool  hovering  = snap->hover_local_x >= 0.0f;
	float along_hit = vertical ? snap->hover_local_y : snap->hover_local_x;

	for (int i = 0; i < visible; i++) {
		int      index = snap->u.pips.window_start + i;
		float    along = strip0 + ( float ) i * PIP_CROSS;
		FluxRect slot  = pips_slot(vertical, bounds, along, PIP_CROSS);

		bool      over     = hovering && along_hit >= along && along_hit < along + PIP_CROSS;
		bool      selected = index == snap->u.pips.selected;
		bool      pressed  = over && index == snap->u.pips.pressed_pip;
		float     d        = selected || (over && !pressed) ? 6.0f : 4.0f;
		FluxColor fg       = !state->enabled ? t->ctrl_strong_fill_disabled
		        : over                       ? t->text_secondary
		                                     : t->ctrl_strong_fill_default;
		pips_draw_dot(rc, &slot, d, fg);
	}

	if (!nav) return;

	float    end  = strip0 + ( float ) visible * PIP_CROSS;
	FluxRect prev = pips_slot(vertical, bounds, 0.0f, PIP_NAV);
	FluxRect next = pips_slot(vertical, bounds, end, PIP_NAV);

	bool prev_en = snap->u.pips.selected > 0;
	bool next_en = snap->u.pips.selected < snap->u.pips.count - 1;
	bool over_p  = hovering && along_hit < PIP_NAV;
	bool over_n  = hovering && along_hit >= end && along_hit < end + PIP_NAV;

	/* The caret glyph is a discrete per-axis symbol (not geometry), so its
	 * orientation pick stays inline. */
	if (prev_en || snap->u.pips.nav_vis == 1)
		pips_draw_caret(
		  rc, t, &prev, vertical ? L"\xEDDB" : L"\xEDD9", over_p, over_p && snap->u.pips.pressed_nav == 1, prev_en
		);
	if (next_en || snap->u.pips.nav_vis == 1)
		pips_draw_caret(
		  rc, t, &next, vertical ? L"\xEDDC" : L"\xEDDA", over_n, over_n && snap->u.pips.pressed_nav == 2, next_en
		);
}
