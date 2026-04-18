#include "../flux_render_internal.h"
#include "../flux_anim.h"
#include "../flux_scroll_geom.h"
#include "fluxent/flux_text.h"
#include <math.h>
#include <string.h>

/* ScrollView — WinUI "conscious scrollbar":
 *   - At rest: 2 px "mini" panning indicator at ~0.45 alpha.
 *   - On cursor near edge / active scroll / drag: expands to full 12 px
 *     track with LineUp + LineDown arrow buttons and a draggable Thumb.
 *   - Fades back to mini ~0.6 s after last activity.
 *
 * Reference:
 *   microsoft-ui-xaml/src/controls/dev/CommonStyles/ScrollBar_themeresources.xaml
 *   microsoft-ui-xaml/src/dxaml/xcp/dxaml/themes/generic.xaml (ScrollBar)
 */

void flux_draw_scroll(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	flux_fill_background(rc, snap, bounds);
}

/* Expansion target (0..1) based on snapshot interaction state. */
static float scroll_target_expansion(FluxRenderSnapshot const *snap, FluxRect const *bounds, double now) {
	if (snap->scroll_drag_axis != 0
		|| snap->scroll_v_up_pressed
		|| snap->scroll_v_dn_pressed
		|| snap->scroll_h_lf_pressed
		|| snap->scroll_h_rg_pressed)
		return 1.0f;

	double const HOLD = 0.6, FADE = 0.4;
	float        act = 0.0f;
		if (snap->scroll_last_activity_time > 0.0) {
			double dt = now - snap->scroll_last_activity_time;
			if (dt < HOLD) act = 1.0f;
			else if (dt < HOLD + FADE) act = 1.0f - ( float ) ((dt - HOLD) / FADE);
		}

	float hover = 0.0f;
		if (snap->scroll_mouse_over) {
			float const EDGE = 24.0f;
			float       vx   = snap->scroll_mouse_local_x;
			float       vy   = snap->scroll_mouse_local_y;
			float       dx   = bounds->w - vx;
				if (dx >= 0.0f && dx < EDGE) {
					float h = 1.0f - (dx / EDGE);
					if (h > hover) hover = h;
				}
			float dy = bounds->h - vy;
				if (dy >= 0.0f && dy < EDGE) {
					float h = 1.0f - (dy / EDGE);
					if (h > hover) hover = h;
				}
		}

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

/* Segoe Fluent Icons arrow glyphs used by WinUI ScrollBar template.
 * Mapping comes directly from ScrollBar_themeresources.xaml:
 *   VerticalDecrementTemplate (up)     = U+EDDB
 *   VerticalIncrementTemplate (down)   = U+EDDC
 *   HorizontalDecrementTemplate (left) = U+EDD9
 *   HorizontalIncrementTemplate (right)= U+EDDA */
static char const GLYPH_UP []    = "\xEE\xB7\x9B"; /* U+EDDB */
static char const GLYPH_DOWN []  = "\xEE\xB7\x9C"; /* U+EDDC */
static char const GLYPH_LEFT []  = "\xEE\xB7\x99"; /* U+EDD9 */
static char const GLYPH_RIGHT [] = "\xEE\xB7\x9A"; /* U+EDDA */

void flux_draw_scroll_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t        = rc->theme;

	float                  target   = scroll_target_expansion(snap, bounds, rc->now);
	FluxCacheEntry        *ce       = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	float                  expanded = target;
		if (ce) {
			FluxTween *a = &ce->scroll_anim_y;
				if (!a->initialized) {
					a->current     = target;
					a->initialized = true;
				}
			float k = 1.0f - expf(-rc->dt * 16.0f);
			if (k > 1.0f) k = 1.0f;
			a->current += (target - a->current) * k;
			expanded    = a->current;
			if (rc->animations_active && (fabsf(target - expanded) > 0.001f || target > 0.0f))
				*rc->animations_active = true;
		}

	FluxScrollBarGeom g;
	flux_scroll_geom_compute(
	  &g, bounds, snap->scroll_content_w, snap->scroll_content_h, snap->scroll_x, snap->scroll_y, expanded
	);

	bool show_v = g.has_v && snap->scroll_v_vis != FLUX_SCROLL_NEVER;
	bool show_h = g.has_h && snap->scroll_h_vis != FLUX_SCROLL_NEVER;
	if (!show_v && !show_h) return;

	FluxColor track_bg     = t ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 20);
	FluxColor thumb_col    = t ? t->ctrl_strong_fill_default : flux_color_rgba(0, 0, 0, 140);
	FluxColor thumb_hover  = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 170);
	FluxColor thumb_press  = t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 120);
	FluxColor arrow_col    = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 170);
	FluxColor arrow_press  = t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 120);
	FluxColor btn_hover_bg = t ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 30);
	FluxColor btn_press_bg = t ? t->subtle_fill_tertiary : flux_color_rgba(0, 0, 0, 20);

	float     mx           = snap->scroll_mouse_local_x + bounds->x;
	float     my           = snap->scroll_mouse_local_y + bounds->y;
	bool      mouse_valid  = snap->scroll_mouse_over != 0;

		/* ── Vertical bar ───────────────────────────────────────────────── */
		if (show_v) {
				if (expanded > 0.05f) {
					FluxColor tb = with_alpha_mul(track_bg, expanded * 0.35f);
					flux_fill_rect(rc, &g.v_bar, tb);
				}

				if (g.v_up_btn.h > 0.5f) {
					bool up_hover   = mouse_valid && flux_rect_contains(&g.v_up_btn, mx, my);
					bool up_pressed = snap->scroll_v_up_pressed;
					if (up_pressed) flux_fill_rect(rc, &g.v_up_btn, with_alpha_mul(btn_press_bg, expanded));
					else if (up_hover) flux_fill_rect(rc, &g.v_up_btn, with_alpha_mul(btn_hover_bg, expanded));
					FluxColor ac = up_pressed ? arrow_press : arrow_col;
					draw_arrow_glyph(rc, &g.v_up_btn, GLYPH_UP, with_alpha_mul(ac, expanded));
				}
				if (g.v_dn_btn.h > 0.5f) {
					bool dn_hover   = mouse_valid && flux_rect_contains(&g.v_dn_btn, mx, my);
					bool dn_pressed = snap->scroll_v_dn_pressed;
					if (dn_pressed) flux_fill_rect(rc, &g.v_dn_btn, with_alpha_mul(btn_press_bg, expanded));
					else if (dn_hover) flux_fill_rect(rc, &g.v_dn_btn, with_alpha_mul(btn_hover_bg, expanded));
					FluxColor ac = dn_pressed ? arrow_press : arrow_col;
					draw_arrow_glyph(rc, &g.v_dn_btn, GLYPH_DOWN, with_alpha_mul(ac, expanded));
				}

			/* Thumb — visible width 8 px (ScrollBarVerticalThumbMinWidth),
			 * right-aligned in mini state, centered in expanded state.
			 * Hit-test rect in g.v_thumb is the full bar width (12 px) for
			 * easy grabbing — only the visual is thinner. */
			FluxRect  thumb   = g.v_thumb;
			bool      t_hover = mouse_valid && flux_rect_contains(&g.v_thumb, mx, my);
			bool      t_drag  = (snap->scroll_drag_axis == 1);
			FluxColor tc      = thumb_col;
			if (t_drag) tc = thumb_press;
			else if (t_hover && expanded > 0.5f) tc = thumb_hover;

			float const THUMB_W  = 8.0f;
			float       w_visual = FLUX_SCROLLBAR_MINI_SIZE + (THUMB_W - FLUX_SCROLLBAR_MINI_SIZE) * expanded;
			float       right_x  = g.v_bar.x + g.v_bar.w - w_visual - 1.0f;
			float       center_x = g.v_bar.x + (g.v_bar.w - w_visual) * 0.5f;
			thumb.x              = right_x + (center_x - right_x) * expanded;
			thumb.w              = w_visual;

			float mini_k         = 1.0f - expanded;
				if (mini_k > 0.0f) {
					float pad = 4.0f * mini_k;
					thumb.y   = g.v_thumb.y + pad;
					thumb.h   = g.v_thumb.h - 2.0f * pad;
					if (thumb.h < 4.0f) thumb.h = 4.0f;
				}
			float thumb_alpha = 0.45f + 0.55f * expanded;
			if (t_drag || t_hover) thumb_alpha = 1.0f;
			FluxColor tc_a = with_alpha_mul(tc, thumb_alpha);
			flux_fill_rounded_rect(rc, &thumb, thumb.w * 0.5f, tc_a);
		}

		/* ── Horizontal bar ─────────────────────────────────────────────── */
		if (show_h) {
				if (expanded > 0.05f) {
					FluxColor tb = with_alpha_mul(track_bg, expanded * 0.35f);
					flux_fill_rect(rc, &g.h_bar, tb);
				}

				if (g.h_lf_btn.w > 0.5f) {
					bool lf_hover   = mouse_valid && flux_rect_contains(&g.h_lf_btn, mx, my);
					bool lf_pressed = snap->scroll_h_lf_pressed;
					if (lf_pressed) flux_fill_rect(rc, &g.h_lf_btn, with_alpha_mul(btn_press_bg, expanded));
					else if (lf_hover) flux_fill_rect(rc, &g.h_lf_btn, with_alpha_mul(btn_hover_bg, expanded));
					FluxColor ac = lf_pressed ? arrow_press : arrow_col;
					draw_arrow_glyph(rc, &g.h_lf_btn, GLYPH_LEFT, with_alpha_mul(ac, expanded));
				}
				if (g.h_rg_btn.w > 0.5f) {
					bool rg_hover   = mouse_valid && flux_rect_contains(&g.h_rg_btn, mx, my);
					bool rg_pressed = snap->scroll_h_rg_pressed;
					if (rg_pressed) flux_fill_rect(rc, &g.h_rg_btn, with_alpha_mul(btn_press_bg, expanded));
					else if (rg_hover) flux_fill_rect(rc, &g.h_rg_btn, with_alpha_mul(btn_hover_bg, expanded));
					FluxColor ac = rg_pressed ? arrow_press : arrow_col;
					draw_arrow_glyph(rc, &g.h_rg_btn, GLYPH_RIGHT, with_alpha_mul(ac, expanded));
				}

			FluxRect  thumb   = g.h_thumb;
			bool      t_hover = mouse_valid && flux_rect_contains(&g.h_thumb, mx, my);
			bool      t_drag  = (snap->scroll_drag_axis == 2);
			FluxColor tc      = thumb_col;
			if (t_drag) tc = thumb_press;
			else if (t_hover && expanded > 0.5f) tc = thumb_hover;

			float const THUMB_H  = 8.0f;
			float       h_visual = FLUX_SCROLLBAR_MINI_SIZE + (THUMB_H - FLUX_SCROLLBAR_MINI_SIZE) * expanded;
			float       bottom_y = g.h_bar.y + g.h_bar.h - h_visual - 1.0f;
			float       center_y = g.h_bar.y + (g.h_bar.h - h_visual) * 0.5f;
			thumb.y              = bottom_y + (center_y - bottom_y) * expanded;
			thumb.h              = h_visual;

			float mini_k         = 1.0f - expanded;
				if (mini_k > 0.0f) {
					float pad = 4.0f * mini_k;
					thumb.x   = g.h_thumb.x + pad;
					thumb.w   = g.h_thumb.w - 2.0f * pad;
					if (thumb.w < 4.0f) thumb.w = 4.0f;
				}
			float thumb_alpha = 0.45f + 0.55f * expanded;
			if (t_drag || t_hover) thumb_alpha = 1.0f;
			FluxColor tc_a = with_alpha_mul(tc, thumb_alpha);
			flux_fill_rounded_rect(rc, &thumb, thumb.h * 0.5f, tc_a);
		}
}
