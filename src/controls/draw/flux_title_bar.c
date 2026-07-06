/**
 * @file flux_title_bar.c
 * @brief TitleBar renderer: an optional back and pane-toggle button (subtle
 * hover/press chrome, Segoe Fluent glyphs), an optional app icon, then the
 * title and a secondary subtitle. Element geometry comes from
 * flux_titlebar_layout, the same source hit-testing uses.
 */
#include "controls/draw/flux_button_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_render_internal.h"

#include "fluxent/controls/flux_title_bar_data.h"

#include <string.h>

static void titlebar_button(
  FluxRenderContext const *rc, FluxRect const *r, wchar_t const *glyph, bool enabled, bool hover, bool press,
  FluxThemeColors const *t
) {
	if (enabled) {
		FluxColor fill = press ? t->subtle_fill_tertiary : hover ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 0);
		if (flux_color_af(fill) > 0.0f) flux_fill_rounded_rect(rc, r, 4.0f, fill);
	}
	flux_button_draw_chevron(rc, r, glyph, 16.0f, enabled ? t->text_primary : t->text_disabled);
}

static void titlebar_label(
  FluxRenderContext const *rc, char const *text, FluxRect const *r, float size, FluxColor color
) {
	if (!rc->text || !text || !text [0]) return;
	FluxTextStyle ts = {0};
	ts.font_size  = size;
	ts.text_align = FLUX_TEXT_LEFT;
	ts.vert_align = FLUX_TEXT_VCENTER;
	ts.color      = color;
	flux_text_draw(rc->text, FLUX_RT(rc), text, r, &ts);
}

/* Back / pane-toggle: shared subtle-fill button placed from its layout rect,
 * hovered when the pointer is inside it (and the button is enabled). */
static void titlebar_nav_button(
  FluxRenderContext const *rc, FluxRect const *bounds, FluxRect const *lr, wchar_t glyph, bool enabled,
  bool pressed_here, float hx, float hy, FluxThemeColors const *t
) {
	FluxRect r     = {bounds->x + lr->x, bounds->y + lr->y, lr->w, lr->h};
	bool     hover = enabled && hx >= lr->x && hx < lr->x + lr->w && hy >= lr->y && hy < lr->y + lr->h;
	wchar_t  g [2] = {glyph, 0};
	titlebar_button(rc, &r, g, enabled, hover, pressed_here, t);
}

static void titlebar_draw_icon(
  FluxRenderContext const *rc, FluxRect const *bounds, FluxRect const *ir, char const *glyph, FluxThemeColors const *t
) {
	if (!rc->text || !glyph) return;
	FluxRect      r  = {bounds->x + ir->x, bounds->y + ir->y, ir->w, ir->h};
	FluxTextStyle ts = {0};
	ts.font_family   = "Segoe Fluent Icons";
	ts.font_size     = FLUX_TB_ICON;
	ts.text_align    = FLUX_TEXT_CENTER;
	ts.vert_align    = FLUX_TEXT_VCENTER;
	ts.color         = t->text_primary;
	flux_text_draw(rc->text, FLUX_RT(rc), glyph, &r, &ts);
}

/* Title, then subtitle offset by the measured title width, both bounded by the
 * caption cluster (when present). */
static void titlebar_draw_titles(
  FluxRenderContext const *rc, FluxRect const *bounds, FluxTitleBarLayout const *l,
  FluxTitleBarSnapshot const *tb, FluxThemeColors const *t
) {
	float title_x = bounds->x + l->title_x;
	float right   = bounds->x + l->content_right - FLUX_TB_PAD;
	if (right <= title_x) return;
	FluxRect tr = {title_x, bounds->y, right - title_x, bounds->h};
	titlebar_label(rc, tb->title, &tr, 14.0f, t->text_primary);
	if (!tb->subtitle || !tb->subtitle [0]) return;
	float    sx = title_x + tb->title_w + FLUX_TB_GAP;
	FluxRect sr = {sx, bounds->y, right - sx, bounds->h};
	if (sr.w > 0.0f) titlebar_label(rc, tb->subtitle, &sr, 14.0f, t->text_secondary);
}

/* One caption button: hover/press fill (close is destructive red with a white
 * glyph), then the glyph. */
static void titlebar_caption_button(
  FluxRenderContext const *rc, FluxRect const *r, wchar_t glyph, bool close, bool hover, bool press,
  FluxThemeColors const *t
) {
	FluxColor glyph_col = t->text_primary;
	if (hover || press) {
		FluxColor fill = close ? (press ? flux_color_rgba(0xC4, 0x2B, 0x1C, 0xE0) : flux_color_rgb(0xC4, 0x2B, 0x1C))
		                       : (press ? t->subtle_fill_tertiary : t->subtle_fill_secondary);
		flux_fill_rect(rc, r, fill);
		if (close) glyph_col = flux_color_rgb(255, 255, 255);
	}
	wchar_t g [2] = {glyph, 0};
	flux_button_draw_chevron(rc, r, g, 10.0f, glyph_col);
}

/* Caption cluster: min, max/restore, close, laid out left-to-right. */
static void titlebar_draw_caption(
  FluxRenderContext const *rc, FluxRect const *bounds, FluxTitleBarLayout const *l,
  FluxTitleBarSnapshot const *tb, float hx, FluxThemeColors const *t
) {
	struct {
		FluxRect const *lr;
		wchar_t         glyph;
		int             region;
	} caps [3] = {
	  {&l->min, ( wchar_t ) FLUX_TB_GLYPH_MIN, FLUX_TB_REGION_MIN},
	  {&l->max, ( wchar_t ) (tb->maximized ? FLUX_TB_GLYPH_REST : FLUX_TB_GLYPH_MAX), FLUX_TB_REGION_MAX},
	  {&l->close, ( wchar_t ) FLUX_TB_GLYPH_CLOSE, FLUX_TB_REGION_CLOSE},
	};
	for (int i = 0; i < 3; i++) {
		FluxRect r     = {bounds->x + caps [i].lr->x, bounds->y + caps [i].lr->y, caps [i].lr->w, caps [i].lr->h};
		bool     hover = hx >= caps [i].lr->x && hx < caps [i].lr->x + caps [i].lr->w;
		titlebar_caption_button(
		  rc, &r, caps [i].glyph, caps [i].region == FLUX_TB_REGION_CLOSE, hover, tb->pressed == caps [i].region, t
		);
	}
}

void flux_draw_title_bar(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const      *t  = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxTitleBarSnapshot const *tb = &snap->u.title_bar;

	flux_fill_background(rc, snap, bounds);

	FluxRect           local = {0.0f, 0.0f, bounds->w, bounds->h};
	FluxTitleBarLayout l
	  = flux_titlebar_layout(&local, tb->show_back, tb->show_pane_toggle, tb->icon_glyph != NULL, tb->caption_buttons);
	float hx = (state->hovered && snap->hover_local_x >= 0.0f) ? snap->hover_local_x : -1.0f;
	float hy = snap->hover_local_y;

	if (l.has_back)
		titlebar_nav_button(
		  rc, bounds, &l.back, ( wchar_t ) FLUX_TB_GLYPH_BACK, !tb->back_disabled, tb->pressed == FLUX_TB_REGION_BACK, hx, hy, t
		);
	if (l.has_toggle)
		titlebar_nav_button(rc, bounds, &l.toggle, ( wchar_t ) FLUX_TB_GLYPH_PANE, true, tb->pressed == FLUX_TB_REGION_PANE, hx, hy, t);
	if (l.has_icon) titlebar_draw_icon(rc, bounds, &l.icon, tb->icon_glyph, t);
	titlebar_draw_titles(rc, bounds, &l, tb, t);
	if (l.has_caption) titlebar_draw_caption(rc, bounds, &l, tb, hx, t);
}
