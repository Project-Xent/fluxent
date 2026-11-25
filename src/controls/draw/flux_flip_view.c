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
	bool      hot    = hovered || pressed;
	FluxColor fg     = hot ? t->text_secondary : t->ctrl_strong_fill_default;
	FluxColor fill   = t->flip_nav_bg_default;
	FluxColor stroke = {0};
	if (t->contrast.active) {
		fg     = hot ? t->contrast.highlight_text : t->contrast.button_text;
		fill   = hot ? t->contrast.highlight : t->contrast.button_face;
		stroke = t->contrast.button_text;
	}
	float size = pressed ? 7.0f : 8.0f;
	return flux_rec_fill_rounded(b, r, FLUX_CORNER_RADIUS, fill)
	    && flux_rec_stroke_rounded(b, r, FLUX_CORNER_RADIUS - 0.5f, stroke, 1.0f)
	    && flux_rec_arrow_wc(b, r, glyph, size, fg);
}

static bool flip_hit(FluxRenderSnap const *snap, FluxRect const *bounds, FluxRect const *r) {
	if (snap->hover_local_x < 0.0f) return false;
	float hx = snap->hover_local_x, hy = snap->hover_local_y;
	return hx >= r->x - bounds->x
	    && hx < r->x - bounds->x + r->w
	    && hy >= r->y - bounds->y
	    && hy < r->y - bounds->y + r->h;
}

static bool flip_rec_nav(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRenderSnap const *snap, FluxRect const *bounds, bool next
) {
	bool     vertical = snap->u.flip.vertical;
	float    span     = vertical ? bounds->h : bounds->w;
	float    edge     = next ? span - FLIP_BTN_W - FLIP_BTN_MARGIN : FLIP_BTN_MARGIN;
	FluxRect r = vertical
	             ? (FluxRect) {bounds->x + (bounds->w - FLIP_BTN_H) * 0.5f, bounds->y + edge, FLIP_BTN_H, FLIP_BTN_W}
	             : (FluxRect) {bounds->x + edge, bounds->y + (bounds->h - FLIP_BTN_H) * 0.5f, FLIP_BTN_W, FLIP_BTN_H};
	static wchar_t const *const arrows [2][2] = {
	  {L"\xEDD9", L"\xEDDA"},
      {L"\xEDDB", L"\xEDDC"}
    };
	bool over    = flip_hit(snap, bounds, &r);
	bool pressed = over && snap->u.flip.pressed_btn == (next ? 2 : 1);
	return flip_rec_button(b, t, &r, arrows [vertical][next], over, pressed);
}

XentDl *flux_flip_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	if (!state->enabled) return NULL;
	FluxThemeColors const *t = flux_rec_theme(rc);
	XentDlBuilder         *b = xent_dl_begin();
	if (!b) return NULL;
	bool ok       = !state->focused || flux_rec_focus_rect(b, t, bounds, FLUX_CORNER_RADIUS);
	bool show_nav = snap->u.flip.buttons_alive || snap->hover_local_x >= 0.0f;
	if (!show_nav) return flux_rec_finish(b, ok);
	ok = ok && (!snap->u.flip.prev_enabled || flip_rec_nav(b, t, snap, bounds, false));
	ok = ok && (!snap->u.flip.next_enabled || flip_rec_nav(b, t, snap, bounds, true));
	return flux_rec_finish(b, ok);
}
