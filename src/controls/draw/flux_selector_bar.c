/**
 * @file flux_selector_bar.c
 * @brief SelectorBarItem renderer: transparent background, icon (0.8-scaled)
 * left of the label, foreground per state (primary / secondary hover /
 * tertiary pressed / disabled), and the accent underline pill at the bottom
 * center (16×3, radius {0.5,1}) growing + fading via pill_t on selection.
 */
#include "controls/draw/flux_button_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_render_internal.h"

#define SBI_PILL_MIN_W 4.0f
#define SBI_PILL_MAX_W 16.0f
#define SBI_PILL_H     3.0f
#define SBI_ICON_SIZE  16.0f

void flux_draw_selector_bar_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t = rc->theme ? rc->theme : flux_theme_default_colors();

	FluxColor fg = !state->enabled ? t->text_disabled
	    : state->pressed && !snap->u.selector.selected ? t->text_tertiary
	    : state->hovered                               ? t->text_secondary
	                                                   : t->text_primary;

	float content_x = bounds->x + 12.0f;
	if (snap->u.selector.icon_glyph) {
		wchar_t  g [2] = {( wchar_t ) snap->u.selector.icon_glyph, 0};
		FluxRect ir    = {content_x, bounds->y, SBI_ICON_SIZE, bounds->h - SBI_PILL_H};
		flux_button_draw_chevron(rc, &ir, g, SBI_ICON_SIZE, fg);
		content_x += SBI_ICON_SIZE + 8.0f;
	}

	if (snap->u.selector.text && rc->text) {
		FluxTextStyle ts;
		memset(&ts, 0, sizeof(ts));
		ts.font_size  = 14.0f;
		ts.text_align = FLUX_TEXT_LEFT;
		ts.vert_align = FLUX_TEXT_VCENTER;
		ts.color      = fg;
		FluxRect tr   = {content_x, bounds->y, bounds->x + bounds->w - 12.0f - content_x, bounds->h - SBI_PILL_H};
		if (tr.w > 0.0f) flux_text_draw(rc->text, FLUX_RT(rc), snap->u.selector.text, &tr, &ts);
	}

	float pt = snap->u.selector.pill_t;
	if (pt > 0.001f) {
		float     w    = SBI_PILL_MIN_W + (SBI_PILL_MAX_W - SBI_PILL_MIN_W) * pt;
		FluxColor pill = state->enabled ? t->accent_default : t->accent_disabled;
		pill.rgba      = (pill.rgba & 0xffffff00u) | ( uint32_t ) (( float ) (pill.rgba & 0xffu) * pt);
		FluxRect r     = {bounds->x + (bounds->w - w) * 0.5f, bounds->y + bounds->h - SBI_PILL_H, w, SBI_PILL_H};
		flux_fill_rounded_rect(rc, &r, 1.0f, pill);
	}

	if (state->focused && state->enabled) {
		FluxRect fr = {bounds->x - 2.0f, bounds->y - 2.0f, bounds->w + 4.0f, bounds->h + 4.0f};
		flux_draw_focus_rect(rc, &fr, FLUX_CORNER_RADIUS);
	}
}
