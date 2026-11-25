#include "controls/draw/flux_control_record_internal.h"

#include "controls/data/flux_title_bar_data.h"

static bool titlebar_hit(float hx, float hy, FluxRect const *r) {
	return hx >= r->x && hx < r->x + r->w && hy >= r->y && hy < r->y + r->h;
}

static FluxRecButtonColors
titlebar_contrast_button(FluxContrastColors const *p, bool inactive, bool enabled, bool hover, bool press) {
	if (!enabled) return (FluxRecButtonColors) {.fill = p->button_face, .text = p->gray_text};
	if (hover || press) return (FluxRecButtonColors) {.fill = p->highlight, .text = p->highlight_text};
	return (FluxRecButtonColors) {.fill = p->button_face, .text = inactive ? p->gray_text : p->button_text};
}

static FluxRecButtonColors
titlebar_button_colors(FluxThemeColors const *t, bool inactive, bool enabled, bool hover, bool press) {
	if (t->contrast.active) return titlebar_contrast_button(&t->contrast, inactive, enabled, hover, press);
	if (!enabled) return (FluxRecButtonColors) {.fill = t->ctrl_fill_disabled, .text = t->text_disabled};
	if (press) return (FluxRecButtonColors) {.fill = t->subtle_fill_tertiary, .text = t->text_secondary};
	if (hover) return (FluxRecButtonColors) {.fill = t->subtle_fill_secondary, .text = t->text_primary};
	return (FluxRecButtonColors) {.text = inactive ? t->text_tertiary : t->text_primary};
}

static bool titlebar_nav_button(
  XentDlBuilder *b, FluxRect const *bounds, FluxRect const *lr, wchar_t glyph, bool enabled, bool pressed_here,
  FluxPoint pointer, FluxThemeColors const *t, bool inactive, bool focused
) {
	FluxRect            r     = {bounds->x + lr->x, bounds->y + lr->y, lr->w, lr->h};
	bool                hover = enabled && titlebar_hit(pointer.x, pointer.y, lr);
	wchar_t             g [2] = {glyph, 0};
	FluxRecButtonColors c     = titlebar_button_colors(t, inactive, enabled, hover, pressed_here);
	return flux_rec_fill_rounded(b, &r, FLUX_CORNER_RADIUS, c.fill)
	    && flux_rec_arrow_wc(b, &r, g, 16.0f, c.text)
	    && (!enabled || !focused || flux_rec_focus_rect(b, t, &r, FLUX_CORNER_RADIUS));
}

static bool titlebar_draw_icon(
  XentDlBuilder *b, FluxRect const *bounds, FluxRect const *ir, FluxTitleBarSnapshot const *tb, FluxThemeColors const *t
) {
	char const *glyph = tb->icon_glyph;
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
	if (tb->inactive && !xent_dl_pushopacity(b, 0.5f)) return false;
	bool ok = flux_rec_text_ex(b, &r, glyph, &style, t->text_primary);
	return (!tb->inactive || xent_dl_popopacity(b)) && ok;
}

static FluxColor titlebar_title_ink(FluxThemeColors const *t, bool inactive, bool subtitle) {
	if (t->contrast.active) {
		FluxColor ink = subtitle ? t->contrast.highlight_text : t->contrast.button_text;
		return inactive ? t->contrast.gray_text : ink;
	}
	if (inactive) return t->text_tertiary;
	return subtitle ? t->text_secondary : t->text_primary;
}

static bool titlebar_draw_text(
  XentDlBuilder *b, FluxRect const *bounds, FluxTitleBarLayout const *layout, FluxTitleBarSnapshot const *tb,
  FluxThemeColors const *t, bool subtitle
) {
	char const *text = subtitle ? tb->subtitle : tb->title;
	if (tb->frame.compact_text || !text || !text [0]) return true;
	float    width = subtitle ? tb->subtitle_w : tb->title_w;
	FluxRect r = flux_titlebar_text_rect(layout, bounds->h, tb->title_w, width, tb->title && tb->title [0], subtitle);
	if (r.w <= 0.0f) return true;
	r.x                 += bounds->x;
	r.y                 += bounds->y;
	XentTextStyle style  = {
	  .font_size   = FLUX_TB_TEXT_SIZE,
	  .font_weight = 400u,
	  .v_align     = 1u,
	  .trim        = r.w < width ? XENT_TEXT_TRIM_CHAR_ELLIPSIS : XENT_TEXT_TRIM_NONE};
	return flux_rec_text_ex(b, &r, text, &style, titlebar_title_ink(t, tb->inactive, subtitle));
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
  XentDlBuilder *b, FluxRect const *bounds, FluxTitleBarLayout const *l, FluxTitleBarSnapshot const *tb,
  FluxPoint pointer, FluxThemeColors const *t
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
		bool     hover = titlebar_hit(pointer.x, pointer.y, caps [i].lr);
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
	  &local, tb->show_back, tb->show_pane_toggle, tb->icon_glyph && tb->icon_glyph [0], &tb->frame
	);
	if (!flux_rec_fill_bg(b, snap, bounds)) return false;
	FluxPoint pointer = {state->hovered ? snap->hover_local_x : -1.0f, snap->hover_local_y};

	if (l.has_back
	    && !titlebar_nav_button(
		  b, bounds, &l.back, ( wchar_t ) FLUX_TB_GLYPH_BACK, state->enabled && !tb->back_disabled,
		  tb->pressed == FLUX_TB_REGION_BACK, pointer, t, tb->inactive,
		  state->focused && tb->focus_part == FLUX_TB_REGION_BACK
		))
		return false;
	if (l.has_toggle
	    && !titlebar_nav_button(
		  b, bounds, &l.toggle, ( wchar_t ) FLUX_TB_GLYPH_PANE, state->enabled, tb->pressed == FLUX_TB_REGION_PANE,
		  pointer, t, tb->inactive, state->focused && tb->focus_part == FLUX_TB_REGION_PANE
		))
		return false;
	if (l.has_icon && !titlebar_draw_icon(b, bounds, &l.icon, tb, t)) return false;
	if (!titlebar_draw_text(b, bounds, &l, tb, t, false) || !titlebar_draw_text(b, bounds, &l, tb, t, true))
		return false;
	if (l.has_caption && !titlebar_draw_caption(b, bounds, &l, tb, pointer, t)) return false;
	return true;
}
