/**
 * @file flux_scroll.c
 * @brief ScrollView overlay → XentDl (per-frame record).
 */
#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_anim.h"
#include "render/flux_scroll_geom.h"

#include <math.h>
#include <string.h>

static bool scroll_is_active(FluxRenderSnap const *snap) {
	return snap->u.scroll.drag_axis != 0
	    || snap->u.scroll.v_up_pressed
	    || snap->u.scroll.v_dn_pressed
	    || snap->u.scroll.h_lf_pressed
	    || snap->u.scroll.h_rg_pressed;
}

static float scroll_activity_expansion(FluxRenderSnap const *snap, double now) {
	double const HOLD = 0.6, FADE = 0.4;
	if (snap->u.scroll.last_activity_time <= 0.0) return 0.0f;
	double dt = now - snap->u.scroll.last_activity_time;
	if (dt < HOLD) return 1.0f;
	if (dt < HOLD + FADE) return 1.0f - ( float ) ((dt - HOLD) / FADE);
	return 0.0f;
}

static float scroll_edge_hover(float distance, float edge) {
	if (distance < 0.0f || distance >= edge) return 0.0f;
	return 1.0f - (distance / edge);
}

static float scroll_hover_expansion(FluxRenderSnap const *snap, FluxRect const *bounds) {
	if (!snap->u.scroll.mouse_over) return 0.0f;
	float const EDGE = 24.0f;
	float       hx   = scroll_edge_hover(bounds->w - snap->u.scroll.mouse_local_x, EDGE);
	float       hy   = scroll_edge_hover(bounds->h - snap->u.scroll.mouse_local_y, EDGE);
	return hx > hy ? hx : hy;
}

static float scroll_target_expansion(FluxRenderSnap const *snap, FluxRect const *bounds, double now) {
	if (scroll_is_active(snap)) return 1.0f;
	float act   = scroll_activity_expansion(snap, now);
	float hover = scroll_hover_expansion(snap, bounds);
	return act > hover ? act : hover;
}

static float scroll_animated_expansion(FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	float           target = scroll_target_expansion(snap, bounds, rc->now);
	FluxCacheEntry *ce     = rc->cache ? flux_rcache_ensure(rc->cache, snap->id) : NULL;
	if (!ce) return target;
	FluxTween *a = &ce->scroll_anim_y;
	if (!a->initialized) {
		a->current     = target;
		a->initialized = true;
	}
	float k = 1.0f - expf(-rc->dt * 16.0f);
	if (k > 1.0f) k = 1.0f;
	a->current += (target - a->current) * k;
	if (rc->animations_active && (fabsf(target - a->current) > 0.001f || target > 0.0f)) *rc->animations_active = true;
	return a->current;
}

static FluxColor with_alpha_mul(FluxColor c, float mul) {
	if (mul < 0.0f) mul = 0.0f;
	if (mul > 1.0f) mul = 1.0f;
	uint8_t a  = ( uint8_t ) (c.rgba & 0xff);
	uint8_t na = ( uint8_t ) (( float ) a * mul);
	return (FluxColor) {(c.rgba & 0xffffff00u) | na};
}

typedef struct {
	FluxColor track_bg, thumb, thumb_hover, thumb_press, arrow, arrow_press, btn_hover_bg, btn_press_bg;
} ScrollOverlayColors;

typedef struct {
	XentDlBuilder           *b;
	FluxRenderSnap const    *snap;
	FluxScrollBarGeom const *geom;
	float                    mx, my;
	bool                     mouse_ok;
	float                    expanded;
	ScrollOverlayColors      colors;
} ScrollRec;

static ScrollOverlayColors scroll_overlay_colors(FluxThemeColors const *t) {
	return (ScrollOverlayColors) {
	  t ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 20),
	  t ? t->ctrl_strong_fill_default : flux_color_rgba(0, 0, 0, 140),
	  t ? t->text_secondary : flux_color_rgba(0, 0, 0, 170),
	  t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 120),
	  t ? t->text_secondary : flux_color_rgba(0, 0, 0, 170),
	  t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 120),
	  t ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 30),
	  t ? t->subtle_fill_tertiary : flux_color_rgba(0, 0, 0, 20),
	};
}

static bool rec_button(ScrollRec const *dc, FluxRect const *r, wchar_t const *glyph, bool hovered, bool pressed) {
	if (r->w <= 0.5f || r->h <= 0.5f) return true;
	bool ok = true;
	if (pressed) ok = flux_rec_fill_rect(dc->b, r, with_alpha_mul(dc->colors.btn_press_bg, dc->expanded));
	else if (hovered) ok = flux_rec_fill_rect(dc->b, r, with_alpha_mul(dc->colors.btn_hover_bg, dc->expanded));
	FluxColor arrow = pressed ? dc->colors.arrow_press : dc->colors.arrow;
	/* Original draw_arrow_glyph uses Segoe Fluent/MDL2 + CENTER/VCENTER; flux_rec_text
	 * would paint the default UI font (tofu) and left-align. */
	return ok && flux_rec_glyph_wc(dc->b, r, glyph, 8.0f, with_alpha_mul(arrow, dc->expanded));
}

static void scroll_apply_mini_padding(FluxRect *thumb, FluxRect const *base, bool vertical, float expanded) {
	float mini_k = 1.0f - expanded;
	if (mini_k <= 0.0f) return;
	float pad = 4.0f * mini_k;
	if (vertical) {
		thumb->y = base->y + pad;
		thumb->h = base->h - 2.0f * pad;
		if (thumb->h < 4.0f) thumb->h = 4.0f;
		return;
	}
	thumb->x = base->x + pad;
	thumb->w = base->w - 2.0f * pad;
	if (thumb->w < 4.0f) thumb->w = 4.0f;
}

static bool rec_thumb(ScrollRec const *dc, FluxRect const *bar, FluxRect const *base_thumb, int axis) {
	bool      vertical = axis == 1;
	FluxRect  thumb    = *base_thumb;
	bool      hover    = dc->mouse_ok && flux_rect_contains(base_thumb, dc->mx, dc->my);
	bool      drag     = dc->snap->u.scroll.drag_axis == axis;
	FluxColor tc       = dc->colors.thumb;
	if (drag) tc = dc->colors.thumb_press;
	else if (hover && dc->expanded > 0.5f) tc = dc->colors.thumb_hover;

	float visual = FLUX_SCROLLBAR_MINI_SIZE + (8.0f - FLUX_SCROLLBAR_MINI_SIZE) * dc->expanded;
	if (vertical) {
		float right_x  = bar->x + bar->w - visual - 1.0f;
		float center_x = bar->x + (bar->w - visual) * 0.5f;
		thumb.x        = right_x + (center_x - right_x) * dc->expanded;
		thumb.w        = visual;
	}
	else {
		float bottom_y = bar->y + bar->h - visual - 1.0f;
		float center_y = bar->y + (bar->h - visual) * 0.5f;
		thumb.y        = bottom_y + (center_y - bottom_y) * dc->expanded;
		thumb.h        = visual;
	}
	scroll_apply_mini_padding(&thumb, base_thumb, vertical, dc->expanded);
	float thumb_alpha = 0.45f + 0.55f * dc->expanded;
	if (drag || hover) thumb_alpha = 1.0f;
	float radius = (vertical ? thumb.w : thumb.h) * 0.5f;
	return flux_rec_fill_rounded(dc->b, &thumb, radius, with_alpha_mul(tc, thumb_alpha));
}

static bool rec_vertical(ScrollRec const *dc) {
	FluxScrollBarGeom const *g  = dc->geom;
	bool                     ok = true;
	if (dc->expanded > 0.05f)
		ok = flux_rec_fill_rect(dc->b, &g->v_bar, with_alpha_mul(dc->colors.track_bg, dc->expanded * 0.35f));
	bool up_h = dc->mouse_ok && flux_rect_contains(&g->v_up_btn, dc->mx, dc->my);
	bool dn_h = dc->mouse_ok && flux_rect_contains(&g->v_dn_btn, dc->mx, dc->my);
	ok        = ok && rec_button(dc, &g->v_up_btn, L"\xEDDB", up_h, dc->snap->u.scroll.v_up_pressed);
	ok        = ok && rec_button(dc, &g->v_dn_btn, L"\xEDDC", dn_h, dc->snap->u.scroll.v_dn_pressed);
	return ok && rec_thumb(dc, &g->v_bar, &g->v_thumb, 1);
}

static bool rec_horizontal(ScrollRec const *dc) {
	FluxScrollBarGeom const *g  = dc->geom;
	bool                     ok = true;
	if (dc->expanded > 0.05f)
		ok = flux_rec_fill_rect(dc->b, &g->h_bar, with_alpha_mul(dc->colors.track_bg, dc->expanded * 0.35f));
	bool lf_h = dc->mouse_ok && flux_rect_contains(&g->h_lf_btn, dc->mx, dc->my);
	bool rg_h = dc->mouse_ok && flux_rect_contains(&g->h_rg_btn, dc->mx, dc->my);
	ok        = ok && rec_button(dc, &g->h_lf_btn, L"\xEDD9", lf_h, dc->snap->u.scroll.h_lf_pressed);
	ok        = ok && rec_button(dc, &g->h_rg_btn, L"\xEDDA", rg_h, dc->snap->u.scroll.h_rg_pressed);
	return ok && rec_thumb(dc, &g->h_bar, &g->h_thumb, 2);
}

XentDl *flux_scroll_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	float               expanded = scroll_animated_expansion(rc, snap, bounds);
	FluxScrollBarGeom   g;
	FluxScrollGeomInput input
	  = {bounds, snap->u.scroll.content_w, snap->u.scroll.content_h, snap->u.scroll.x, snap->u.scroll.y, expanded};
	flux_scroll_geom_compute(&g, &input);
	bool show_v = g.has_v && snap->u.scroll.v_vis != FLUX_SCROLL_NEVER;
	bool show_h = g.has_h && snap->u.scroll.h_vis != FLUX_SCROLL_NEVER;
	if (!show_v && !show_h) {
		XentDlBuilder *b = xent_dl_begin();
		return b ? xent_dl_end(b) : NULL;
	}

	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	ScrollRec dc = {
	  .b        = b,
	  .snap     = snap,
	  .geom     = &g,
	  .mx       = snap->u.scroll.mouse_local_x,
	  .my       = snap->u.scroll.mouse_local_y,
	  .mouse_ok = snap->u.scroll.mouse_over != 0,
	  .expanded = expanded,
	  .colors   = scroll_overlay_colors(flux_rec_theme(rc)),
	};
	bool ok = true;
	if (show_v) ok = rec_vertical(&dc);
	if (ok && show_h) ok = rec_horizontal(&dc);
	return flux_rec_finish(b, ok);
}
