/**
 * @file flux_titlebar_record.c
 * @brief TitleBar recorder: back/pane-toggle buttons, app icon, title/subtitle,
 * and the caption cluster. Element geometry comes from flux_titlebar_layout,
 * the same source hit-testing uses.
 */
#include "controls/draw/flux_control_record_internal.h"

#include "controls/data/flux_title_bar_data.h"

static bool titlebar_hit(float hx, float hy, FluxRect const *r) {
	return hx >= r->x && hx < r->x + r->w && hy >= r->y && hy < r->y + r->h;
}

static bool titlebar_button(
  XentDlBuilder *b, FluxRect const *r, wchar_t const *glyph, bool enabled, bool hover, bool press,
  FluxThemeColors const *t
) {
	if (enabled) {
		FluxColor fill = press ? t->subtle_fill_tertiary
		               : hover ? t->subtle_fill_secondary
		                       : flux_color_rgba(0, 0, 0, 0);
		if (flux_color_af(fill) > 0.0f && !flux_rec_fill_rounded(b, r, 4.0f, fill)) return false;
	}
	return flux_rec_glyph_wc(b, r, glyph, 16.0f, enabled ? t->text_primary : t->text_disabled);
}

static bool titlebar_nav_button(
  XentDlBuilder *b, FluxRect const *bounds, FluxRect const *lr, wchar_t glyph, bool enabled, bool pressed_here,
  float hx, float hy, FluxThemeColors const *t
) {
	FluxRect r     = {bounds->x + lr->x, bounds->y + lr->y, lr->w, lr->h};
	bool     hover = enabled && titlebar_hit(hx, hy, lr);
	wchar_t  g [2] = {glyph, 0};
	return titlebar_button(b, &r, g, enabled, hover, pressed_here, t);
}

static bool titlebar_draw_icon(
  XentDlBuilder *b, FluxRect const *bounds, FluxRect const *ir, char const *glyph, FluxThemeColors const *t
) {
	if (!glyph || !glyph [0]) return true;
	FluxRect      r     = {bounds->x + ir->x, bounds->y + ir->y, ir->w, ir->h};
	XentTextStyle style = {
	  .font_size   = FLUX_TB_ICON,
	  .font_weight = 400u,
	  .h_align     = 1u,
	  .v_align     = 1u,
	  .flags       = XENT_DL_TEXT_ICON,
	  .font_family = flux_icon_font_family(),
	};
	return flux_rec_text_ex(b, &r, glyph, &style, t->text_primary);
}

static bool titlebar_titles(
  XentDlBuilder *b, FluxRect const *bounds, FluxTitleBarLayout const *l, FluxTitleBarSnapshot const *tb,
  FluxThemeColors const *t
) {
	float title_x = bounds->x + l->title_x;
	float right   = bounds->x + l->content_right - FLUX_TB_PAD;
	if (right <= title_x) return true;
	if (tb->title) {
		FluxRect tr = {title_x, bounds->y, right - title_x, bounds->h};
		if (!flux_rec_text(b, &tr, tb->title, 14.0f, 400u, t->text_primary)) return false;
	}
	if (!tb->subtitle || !tb->subtitle [0]) return true;
	float    sx = title_x + tb->title_w + FLUX_TB_GAP;
	FluxRect sr = {sx, bounds->y, right - sx, bounds->h};
	if (sr.w <= 0.0f) return true;
	return flux_rec_text(b, &sr, tb->subtitle, 14.0f, 400u, t->text_secondary);
}

static bool titlebar_caption_button(
  XentDlBuilder *b, FluxRect const *r, wchar_t glyph, bool close, bool hover, bool press, FluxThemeColors const *t
) {
	FluxColor glyph_col = t->text_primary;
	if (hover || press) {
		FluxColor fill = close ? (press ? flux_color_rgba(0xc4, 0x2b, 0x1c, 0xe0) : flux_color_rgb(0xc4, 0x2b, 0x1c))
		                       : (press ? t->subtle_fill_tertiary : t->subtle_fill_secondary);
		if (!flux_rec_fill_rect(b, r, fill)) return false;
		if (close) glyph_col = flux_color_rgb(255, 255, 255);
	}
	wchar_t g [2] = {glyph, 0};
	return flux_rec_glyph_wc(b, r, g, 10.0f, glyph_col);
}

static bool titlebar_draw_caption(
  XentDlBuilder *b, FluxRect const *bounds, FluxTitleBarLayout const *l, FluxTitleBarSnapshot const *tb, float hx,
  FluxThemeColors const *t
) {
	struct {
		FluxRect const *lr;
		wchar_t         glyph;
		int             region;
	} caps [3] = {
	  {&l->min,   ( wchar_t ) FLUX_TB_GLYPH_MIN,                                        FLUX_TB_REGION_MIN  },
	  {&l->max,   ( wchar_t ) (tb->maximized ? FLUX_TB_GLYPH_REST : FLUX_TB_GLYPH_MAX), FLUX_TB_REGION_MAX  },
	  {&l->close, ( wchar_t ) FLUX_TB_GLYPH_CLOSE,                                      FLUX_TB_REGION_CLOSE},
	};

	for (int i = 0; i < 3; i++) {
		FluxRect r     = {bounds->x + caps [i].lr->x, bounds->y + caps [i].lr->y, caps [i].lr->w, caps [i].lr->h};
		bool     hover = hx >= caps [i].lr->x && hx < caps [i].lr->x + caps [i].lr->w;
		if (!titlebar_caption_button(
			  b, &r, caps [i].glyph, caps [i].region == FLUX_TB_REGION_CLOSE, hover, tb->pressed == caps [i].region, t
			))
			return false;
	}
	return true;
}

bool flux_rec_titlebar(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const      *t     = flux_rec_theme(rc);
	FluxTitleBarSnapshot const *tb    = &snap->u.title_bar;
	FluxRect                    local = {0.0f, 0.0f, bounds->w, bounds->h};
	FluxTitleBarLayout          l     = flux_titlebar_layout(
	  &local, tb->show_back, tb->show_pane_toggle, tb->icon_glyph && tb->icon_glyph [0], tb->caption_buttons
	);
	if (!flux_rec_fill_bg(b, snap, bounds)) return false;
	float hx = state->hovered && snap->hover_local_x >= 0.0f ? snap->hover_local_x : -1.0f;
	float hy = snap->hover_local_y;

	if (l.has_back
		&& !titlebar_nav_button(
		  b, bounds, &l.back, ( wchar_t ) FLUX_TB_GLYPH_BACK, !tb->back_disabled, tb->pressed == FLUX_TB_REGION_BACK,
		  hx, hy, t
		))
		return false;
	if (l.has_toggle
		&& !titlebar_nav_button(
		  b, bounds, &l.toggle, ( wchar_t ) FLUX_TB_GLYPH_PANE, true, tb->pressed == FLUX_TB_REGION_PANE, hx, hy, t
		))
		return false;
	if (l.has_icon && !titlebar_draw_icon(b, bounds, &l.icon, tb->icon_glyph, t)) return false;
	if (!titlebar_titles(b, bounds, &l, tb, t)) return false;
	if (l.has_caption && !titlebar_draw_caption(b, bounds, &l, tb, hx, t)) return false;
	return true;
}
