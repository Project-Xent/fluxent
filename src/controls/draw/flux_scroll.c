#include "render/flux_render_internal.h"
#include "render/flux_anim.h"
#include "render/flux_scroll_geom.h"
#include "fluxent/flux_text.h"
#include <math.h>
#include <string.h>

void flux_draw_scroll(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	flux_fill_background(rc, snap, bounds);
}

static bool scroll_is_active(FluxRenderSnapshot const *snap) {
	return snap->scroll_drag_axis != 0
	    || snap->scroll_v_up_pressed
	    || snap->scroll_v_dn_pressed
	    || snap->scroll_h_lf_pressed
	    || snap->scroll_h_rg_pressed;
}

static float scroll_activity_expansion(FluxRenderSnapshot const *snap, double now) {
	double const HOLD = 0.6, FADE = 0.4;
	if (snap->scroll_last_activity_time <= 0.0) return 0.0f;

	double dt = now - snap->scroll_last_activity_time;
	if (dt < HOLD) return 1.0f;
	if (dt < HOLD + FADE) return 1.0f - ( float ) ((dt - HOLD) / FADE);
	return 0.0f;
}

static float scroll_edge_hover(float distance, float edge) {
	if (distance < 0.0f || distance >= edge) return 0.0f;
	return 1.0f - (distance / edge);
}

static float scroll_hover_expansion(FluxRenderSnapshot const *snap, FluxRect const *bounds) {
	if (!snap->scroll_mouse_over) return 0.0f;

	float const EDGE = 24.0f;
	float       dx   = bounds->w - snap->scroll_mouse_local_x;
	float       dy   = bounds->h - snap->scroll_mouse_local_y;
	float       hx   = scroll_edge_hover(dx, EDGE);
	float       hy   = scroll_edge_hover(dy, EDGE);
	return hx > hy ? hx : hy;
}

static float scroll_target_expansion(FluxRenderSnapshot const *snap, FluxRect const *bounds, double now) {
	if (scroll_is_active(snap)) return 1.0f;

	float act   = scroll_activity_expansion(snap, now);
	float hover = scroll_hover_expansion(snap, bounds);
	return act > hover ? act : hover;
}

static FluxColor with_alpha_mul(FluxColor c, float mul) {
	if (mul < 0.0f) mul = 0.0f;
	if (mul > 1.0f) mul = 1.0f;
	uint8_t a  = ( uint8_t ) (c.rgba & 0xff);
	uint8_t na = ( uint8_t ) (( float ) a * mul);
	return (FluxColor) {(c.rgba & 0xffffff00u) | na};
}

static void draw_arrow_glyph(FluxRenderContext const *rc, FluxRect const *r, char const *glyph_utf8, FluxColor color) {
	if (r->w <= 0.0f || r->h <= 0.0f) return;
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = "Segoe Fluent Icons";
	ts.font_size   = 8.0f;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_CENTER;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = color;
	ts.word_wrap   = false;
	flux_text_draw(rc->text, FLUX_RT(rc), glyph_utf8, r, &ts);
}

static char const GLYPH_UP []    = "\xEE\xB7\x9B";
static char const GLYPH_DOWN []  = "\xEE\xB7\x9C";
static char const GLYPH_LEFT []  = "\xEE\xB7\x99";
static char const GLYPH_RIGHT [] = "\xEE\xB7\x9A";

typedef struct {
	FluxColor track_bg;
	FluxColor thumb;
	FluxColor thumb_hover;
	FluxColor thumb_press;
	FluxColor arrow;
	FluxColor arrow_press;
	FluxColor btn_hover_bg;
	FluxColor btn_press_bg;
} ScrollOverlayColors;

typedef struct {
	float mx;
	float my;
	bool  valid;
} ScrollMouse;

typedef struct {
	FluxRenderContext const  *rc;
	FluxRenderSnapshot const *snap;
	FluxScrollBarGeom const  *geom;
	ScrollMouse               mouse;
	float                     expanded;
	ScrollOverlayColors       colors;
} ScrollDrawContext;

typedef struct {
	FluxRect const *rect;
	char const     *glyph;
	bool            hovered;
	bool            pressed;
} ScrollButton;

static float
scroll_animated_expansion(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds) {
	float           target   = scroll_target_expansion(snap, bounds, rc->now);
	FluxCacheEntry *ce       = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	float           expanded = target;
	if (!ce) return expanded;

	FluxTween *a = &ce->scroll_anim_y;
	if (!a->initialized) {
		a->current     = target;
		a->initialized = true;
	}

	float k = 1.0f - expf(-rc->dt * 16.0f);
	if (k > 1.0f) k = 1.0f;
	a->current += (target - a->current) * k;
	expanded    = a->current;
	if (rc->animations_active && (fabsf(target - expanded) > 0.001f || target > 0.0f)) *rc->animations_active = true;
	return expanded;
}

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

static void draw_scroll_button(ScrollDrawContext const *dc, ScrollButton const *button) {
	FluxRect const *r = button->rect;
	if (r->w <= 0.5f || r->h <= 0.5f) return;
	if (button->pressed) flux_fill_rect(dc->rc, r, with_alpha_mul(dc->colors.btn_press_bg, dc->expanded));
	else if (button->hovered) flux_fill_rect(dc->rc, r, with_alpha_mul(dc->colors.btn_hover_bg, dc->expanded));

	FluxColor arrow = button->pressed ? dc->colors.arrow_press : dc->colors.arrow;
	draw_arrow_glyph(dc->rc, r, button->glyph, with_alpha_mul(arrow, dc->expanded));
}

static FluxColor scroll_thumb_color(ScrollDrawContext const *dc, FluxRect const *thumb, int axis) {
	bool      hovered = dc->mouse.valid && flux_rect_contains(thumb, dc->mouse.mx, dc->mouse.my);
	bool      dragged = dc->snap->scroll_drag_axis == axis;
	FluxColor color   = dc->colors.thumb;
	if (dragged) color = dc->colors.thumb_press;
	else if (hovered && dc->expanded > 0.5f) color = dc->colors.thumb_hover;
	return color;
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

static void draw_scroll_thumb(ScrollDrawContext const *dc, FluxRect const *bar, FluxRect const *base_thumb, int axis) {
	bool      vertical = axis == 1;
	FluxRect  thumb    = *base_thumb;
	bool      hover    = dc->mouse.valid && flux_rect_contains(base_thumb, dc->mouse.mx, dc->mouse.my);
	bool      drag     = dc->snap->scroll_drag_axis == axis;
	FluxColor tc       = scroll_thumb_color(dc, base_thumb, axis);

	float     visual   = FLUX_SCROLLBAR_MINI_SIZE + (8.0f - FLUX_SCROLLBAR_MINI_SIZE) * dc->expanded;
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
	flux_fill_rounded_rect(dc->rc, &thumb, (vertical ? thumb.w : thumb.h) * 0.5f, with_alpha_mul(tc, thumb_alpha));
}

static ScrollButton scroll_button(FluxRect const *rect, char const *glyph, bool hovered, bool pressed) {
	return (ScrollButton) {.rect = rect, .glyph = glyph, .hovered = hovered, .pressed = pressed};
}

static void draw_scroll_vertical(ScrollDrawContext const *dc) {
	FluxScrollBarGeom const *g = dc->geom;
	if (dc->expanded > 0.05f)
		flux_fill_rect(dc->rc, &g->v_bar, with_alpha_mul(dc->colors.track_bg, dc->expanded * 0.35f));

	bool         up_hover = dc->mouse.valid && flux_rect_contains(&g->v_up_btn, dc->mouse.mx, dc->mouse.my);
	bool         dn_hover = dc->mouse.valid && flux_rect_contains(&g->v_dn_btn, dc->mouse.mx, dc->mouse.my);
	ScrollButton up       = scroll_button(&g->v_up_btn, GLYPH_UP, up_hover, dc->snap->scroll_v_up_pressed);
	ScrollButton dn       = scroll_button(&g->v_dn_btn, GLYPH_DOWN, dn_hover, dc->snap->scroll_v_dn_pressed);
	draw_scroll_button(dc, &up);
	draw_scroll_button(dc, &dn);
	draw_scroll_thumb(dc, &g->v_bar, &g->v_thumb, 1);
}

static void draw_scroll_horizontal(ScrollDrawContext const *dc) {
	FluxScrollBarGeom const *g = dc->geom;
	if (dc->expanded > 0.05f)
		flux_fill_rect(dc->rc, &g->h_bar, with_alpha_mul(dc->colors.track_bg, dc->expanded * 0.35f));

	bool         lf_hover = dc->mouse.valid && flux_rect_contains(&g->h_lf_btn, dc->mouse.mx, dc->mouse.my);
	bool         rg_hover = dc->mouse.valid && flux_rect_contains(&g->h_rg_btn, dc->mouse.mx, dc->mouse.my);
	ScrollButton lf       = scroll_button(&g->h_lf_btn, GLYPH_LEFT, lf_hover, dc->snap->scroll_h_lf_pressed);
	ScrollButton rg       = scroll_button(&g->h_rg_btn, GLYPH_RIGHT, rg_hover, dc->snap->scroll_h_rg_pressed);
	draw_scroll_button(dc, &lf);
	draw_scroll_button(dc, &rg);
	draw_scroll_thumb(dc, &g->h_bar, &g->h_thumb, 2);
}

void flux_draw_scroll_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds) {
	float               expanded = scroll_animated_expansion(rc, snap, bounds);
	FluxScrollBarGeom   g;
	FluxScrollGeomInput input
	  = {bounds, snap->scroll_content_w, snap->scroll_content_h, snap->scroll_x, snap->scroll_y, expanded};
	flux_scroll_geom_compute(&g, &input);

	bool show_v = g.has_v && snap->scroll_v_vis != FLUX_SCROLL_NEVER;
	bool show_h = g.has_h && snap->scroll_h_vis != FLUX_SCROLL_NEVER;
	if (!show_v && !show_h) return;

	ScrollMouse mouse
	  = {snap->scroll_mouse_local_x + bounds->x, snap->scroll_mouse_local_y + bounds->y, snap->scroll_mouse_over != 0};
	ScrollOverlayColors colors = scroll_overlay_colors(rc->theme);
	ScrollDrawContext dc = {.rc = rc, .snap = snap, .geom = &g, .mouse = mouse, .expanded = expanded, .colors = colors};
	if (show_v) draw_scroll_vertical(&dc);
	if (show_h) draw_scroll_horizontal(&dc);
}
