/**
 * @file flux_flip_view.c
 * @brief FlipView overlay nav chips → XentDl.
 */
#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_control_draw.h"
#include "render/flux_fluent.h"

#include <string.h>

#define FLIP_BTN_W      16.0f
#define FLIP_BTN_H      38.0f
#define FLIP_BTN_MARGIN 1.0f

static bool flip_rec_button(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r, wchar_t const *glyph, bool hovered, bool pressed
) {
	bool      ok  = flux_rec_fill_rounded(b, r, FLUX_CORNER_RADIUS, t->flip_nav_bg_default);
	FluxColor fg  = hovered || pressed ? t->text_secondary : t->ctrl_strong_fill_default;
	FluxRect  box = *r;
	if (pressed) {
		box.x += r->w * 0.0625f;
		box.y += r->h * 0.0625f;
		box.w *= 0.875f;
		box.h *= 0.875f;
	}
	return ok && flux_rec_glyph_wc(b, &box, glyph, 8.0f, fg);
}

static bool flip_hit(FluxRenderSnap const *snap, FluxRect const *bounds, FluxRect const *r) {
	if (snap->hover_local_x < 0.0f) return false;
	float hx = snap->hover_local_x, hy = snap->hover_local_y;
	return hx >= r->x - bounds->x
	    && hx < r->x - bounds->x + r->w
	    && hy >= r->y - bounds->y
	    && hy < r->y - bounds->y + r->h;
}

XentDl *flux_flip_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxThemeColors const *t        = flux_rec_theme(rc);
	bool                   vertical = snap->u.flip.vertical;
	bool                   hovered  = snap->hover_local_x >= 0.0f;
	bool                   alive    = snap->u.flip.buttons_alive;
	XentDlBuilder         *b        = xent_dl_begin();
	if (!b) return NULL;
	if (!alive && !hovered) return xent_dl_end(b);

	bool ok = true;
	if (snap->u.flip.prev_enabled) {
		FluxRect r
		  = vertical
		    ? (
				FluxRect
			  ) {bounds->x + (bounds->w - FLIP_BTN_H) * 0.5f, bounds->y + FLIP_BTN_MARGIN, FLIP_BTN_H, FLIP_BTN_W}
		    : (FluxRect) {
				bounds->x + FLIP_BTN_MARGIN, bounds->y + (bounds->h - FLIP_BTN_H) * 0.5f, FLIP_BTN_W, FLIP_BTN_H};
		bool over = flip_hit(snap, bounds, &r);
		ok = flip_rec_button(b, t, &r, vertical ? L"\xEDDB" : L"\xEDD9", over, over && snap->u.flip.pressed_btn == 1);
	}
	if (ok && snap->u.flip.next_enabled) {
		FluxRect r
		  = vertical
		    ? (
				FluxRect
			  ) {bounds->x + (bounds->w - FLIP_BTN_H) * 0.5f, bounds->y + bounds->h - FLIP_BTN_W - FLIP_BTN_MARGIN, FLIP_BTN_H, FLIP_BTN_W}
		    : (FluxRect) {
				bounds->x + bounds->w - FLIP_BTN_W - FLIP_BTN_MARGIN, bounds->y + (bounds->h - FLIP_BTN_H) * 0.5f,
				FLIP_BTN_W, FLIP_BTN_H};
		bool over = flip_hit(snap, bounds, &r);
		ok = flip_rec_button(b, t, &r, vertical ? L"\xEDDC" : L"\xEDDA", over, over && snap->u.flip.pressed_btn == 2);
	}
	return flux_rec_finish(b, ok);
}
