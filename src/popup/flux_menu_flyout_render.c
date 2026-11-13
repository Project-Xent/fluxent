#include "flux_menu_internal.h"
#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_dl_replay.h"
#include "render/flux_icon.h"
#include "render/flux_render_internal.h"

#include <string.h>

static char const GLYPH_CHECKMARK [] = "\xEE\x9C\xBE";
static char const GLYPH_CHEVRON_R [] = "\xEE\xA5\xB4";
static char const GLYPH_RADIO_DOT [] = "\xEE\xA4\x95";

typedef enum
{
	VS_NORMAL,
	VS_DISABLED,
	VS_PRESSED,
	VS_ACTIVE,
} ItemVisualState;

typedef struct ResolvedColors {
	FluxColor presenter_bg;
	FluxColor presenter_border;
	FluxColor separator;
	FluxColor fg_primary;
	FluxColor fg_secondary;
	FluxColor fg_tertiary;
	FluxColor fg_disabled;
	FluxColor bg_hover;
	FluxColor bg_pressed;
	FluxColor focus_outer;
	FluxColor focus_inner;
} ResolvedColors;

typedef struct MenuPresenterLayout {
	FluxRect bounds;
	float    content_x0;
	float    content_y0;
	float    content_w;
	float    content_y1;
} MenuPresenterLayout;

typedef struct MenuItemPalette {
	FluxColor bg_fill;
	FluxColor fg_text;
	FluxColor fg_accel;
	FluxColor fg_chevron;
} MenuItemPalette;

typedef struct MenuItemState {
	bool pressed;
	bool submenu_open;
	bool hovered;
	bool focused_kb;
} MenuItemState;

typedef struct MenuRecContext {
	FluxMenu                  *menu;
	XentDlBuilder             *b;
	MenuPresenterLayout const *layout;
	ResolvedColors const      *colors;
} MenuRecContext;

typedef struct MenuTextDraw {
	char const     *text;
	FluxRect const *box;
	float           size;
	FluxColor       color;
	uint8_t         align;
	uint8_t         valign;
	bool            icon_font;
} MenuTextDraw;

typedef struct MenuLabelSpan {
	float text_x;
	float label_right;
} MenuLabelSpan;

static ItemVisualState item_visual_state(StoredItem const *it, MenuItemState const *state) {
	if (!it->enabled) return VS_DISABLED;
	if (state->pressed) return VS_PRESSED;
	if (state->submenu_open || state->hovered || state->focused_kb) return VS_ACTIVE;
	return VS_NORMAL;
}

static void ensure_brush(FluxMenu *m, ID2D1DeviceContext *d2d) {
	if (m->brush || !d2d) return;
	D2D1_COLOR_F          black = {0, 0, 0, 1};
	D2D1_BRUSH_PROPERTIES bp;
	bp.opacity       = 1.0f;
	bp.transform._11 = 1;
	bp.transform._12 = 0;
	bp.transform._21 = 0;
	bp.transform._22 = 1;
	bp.transform._31 = 0;
	bp.transform._32 = 0;
	ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) d2d, &black, &bp, &m->brush);
}

static void resolve_presenter_colors(ResolvedColors *r, FluxMenu const *m, bool dark) {
	( void ) m;
	r->presenter_bg = dark ? flux_color_rgb(0x2c, 0x2c, 0x2c) : flux_color_rgb(0xf9, 0xf9, 0xf9);
	if (dark) r->presenter_border = flux_color_rgba(0, 0, 0, 0x33);
	else r->presenter_border = flux_color_rgba(0, 0, 0, 0x0f);
}

static void resolve_text_colors(ResolvedColors *r, FluxThemeColors const *t, bool dark) {
	r->fg_primary
	  = t ? t->text_primary : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0xff) : flux_color_rgba(0, 0, 0, 0xe4));
	r->fg_secondary
	  = t ? t->text_secondary : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0xc5) : flux_color_rgba(0, 0, 0, 0x9e));
	r->fg_tertiary
	  = t ? t->text_tertiary : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x87) : flux_color_rgba(0, 0, 0, 0x72));
	r->fg_disabled
	  = t ? t->text_disabled : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x5d) : flux_color_rgba(0, 0, 0, 0x5c));
}

static void resolve_state_colors(ResolvedColors *r, FluxThemeColors const *t, bool dark) {
	r->bg_hover = t ? t->subtle_fill_secondary
	                : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x0f) : flux_color_rgba(0, 0, 0, 0x09));
	r->bg_pressed
	  = t ? t->subtle_fill_tertiary : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x0a) : flux_color_rgba(0, 0, 0, 0x06));
	r->focus_outer
	  = t ? t->focus_stroke_outer : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0xff) : flux_color_rgba(0, 0, 0, 0xe4));
	r->focus_inner
	  = t ? t->focus_stroke_inner : (dark ? flux_color_rgba(0, 0, 0, 0xb3) : flux_color_rgba(0xff, 0xff, 0xff, 0xb3));
}

static ResolvedColors resolve_colors(FluxMenu const *m) {
	ResolvedColors         r;
	FluxThemeColors const *t    = NULL;
	bool                   dark = false;
	menu_resolve_theme(m, &t, &dark);

	resolve_presenter_colors(&r, m, dark);
	r.separator = t ? t->divider_stroke_default
	                : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x15) : flux_color_rgba(0, 0, 0, 0x0f));
	resolve_text_colors(&r, t, dark);
	resolve_state_colors(&r, t, dark);
	return r;
}

static bool rec_menu_text(MenuRecContext const *dc, MenuTextDraw const *draw) {
	if (!draw->text || !*draw->text) return true;
	XentTextStyle style = {
	  .font_size   = draw->size,
	  .font_weight = FLUX_FONT_REGULAR,
	  .h_align     = draw->align,
	  .v_align     = draw->valign,
	  .flags       = draw->icon_font ? XENT_DL_TEXT_ICON : 0u,
	  .font_family = draw->icon_font ? flux_icon_font_family() : NULL,
	};
	return flux_rec_text_ex(dc->b, draw->box, draw->text, &style, draw->color);
}

static FluxRenderCtx
menu_render_context(FluxMenu *m, ID2D1DeviceContext *d2d, FluxThemeColors const *theme, bool is_dark) {
	FluxRenderCtx rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d     = d2d;
	rc.brush   = m->brush;
	rc.text    = m->text;
	rc.theme   = theme;
	rc.dpi     = flux_window_dpi(m->owner);
	rc.is_dark = is_dark;
	return rc;
}

static MenuPresenterLayout menu_presenter_layout(FluxMenu *m) {
	MenuPresenterLayout layout;
	layout.bounds     = (FluxRect) {0.0f, 0.0f, m->total_w, m->total_h};
	layout.content_x0 = FLUX_MENU_PRESENTER_BORDER;
	layout.content_y0 = FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_PRESENTER_PAD_TOP;
	layout.content_w  = m->total_w - 2.0f * FLUX_MENU_PRESENTER_BORDER;
	layout.content_y1 = m->total_h - FLUX_MENU_PRESENTER_BORDER - FLUX_MENU_PRESENTER_PAD_BOTTOM;
	return layout;
}

static bool menu_rec_presenter(MenuRecContext const *dc) {
	bool  ok = flux_rec_fill_rounded(dc->b, &dc->layout->bounds, FLUX_MENU_PRESENTER_CORNER, dc->colors->presenter_bg);
	float half     = FLUX_MENU_PRESENTER_BORDER * 0.5f;
	FluxRect inset = {
	  dc->layout->bounds.x + half, dc->layout->bounds.y + half, dc->layout->bounds.w - half * 2.0f,
	  dc->layout->bounds.h - half * 2.0f};
	return ok
	    && flux_rec_stroke_rounded(
		  dc->b, &inset, FLUX_MENU_PRESENTER_CORNER, dc->colors->presenter_border, FLUX_MENU_PRESENTER_BORDER
		);
}

static bool menu_item_submenu_open(FluxMenu *m, StoredItem const *it, int index) {
	if (it->type != FLUX_MENU_ITEM_SUBMENU || index != m->open_submenu_index) return false;
	return m->open_submenu && flux_popup_visible(m->open_submenu->popup);
}

static FluxRect menu_item_rect(StoredItem const *it, MenuPresenterLayout const *layout) {
	float slot_top = layout->content_y0 + it->y;
	float ir_x     = layout->content_x0 + FLUX_MENU_ITEM_MARGIN_H;
	float ir_y     = slot_top + FLUX_MENU_ITEM_MARGIN_V;
	float ir_w     = layout->content_w - 2.0f * FLUX_MENU_ITEM_MARGIN_H;
	float ir_h     = it->h - 2.0f * FLUX_MENU_ITEM_MARGIN_V;
	return (FluxRect) {ir_x, ir_y, ir_w, ir_h};
}

static MenuItemPalette
menu_item_palette(ResolvedColors const *colors, StoredItem const *it, MenuItemState const *state) {
	MenuItemPalette palette = {
	  .bg_fill    = flux_color_rgba(0, 0, 0, 0),
	  .fg_text    = colors->fg_primary,
	  .fg_accel   = colors->fg_secondary,
	  .fg_chevron = colors->fg_secondary,
	};
	ItemVisualState vs = item_visual_state(it, state);
	switch (vs) {
	case VS_DISABLED :
		palette.fg_text    = colors->fg_disabled;
		palette.fg_accel   = colors->fg_disabled;
		palette.fg_chevron = colors->fg_disabled;
		return palette;
	case VS_PRESSED :
		palette.bg_fill    = colors->bg_pressed;
		palette.fg_accel   = colors->fg_tertiary;
		palette.fg_chevron = colors->fg_tertiary;
		return palette;
	case VS_ACTIVE : palette.bg_fill = colors->bg_hover; return palette;
	case VS_NORMAL : return palette;
	}
	return palette;
}

static bool menu_rec_focus_ring(MenuRecContext const *dc, FluxRect const *ir) {
	float const outer_w    = 2.0f;
	float const inner_w    = 1.0f;
	float       oh         = outer_w * 0.5f;
	FluxRect    outer_rect = {ir->x + oh, ir->y + oh, ir->w - outer_w, ir->h - outer_w};
	bool     ok  = flux_rec_stroke_rounded(dc->b, &outer_rect, FLUX_MENU_ITEM_CORNER, dc->colors->focus_outer, outer_w);
	float    ins = outer_w + inner_w * 0.5f;
	FluxRect inner_rect = {ir->x + ins, ir->y + ins, ir->w - 2.0f * ins, ir->h - 2.0f * ins};
	float    inner_r    = FLUX_MENU_ITEM_CORNER - outer_w;
	if (inner_r < 0.0f) inner_r = 0.0f;
	return ok && flux_rec_stroke_rounded(dc->b, &inner_rect, inner_r, dc->colors->focus_inner, inner_w);
}

static float menu_rec_toggle_marker(
  MenuRecContext const *dc, StoredItem const *it, FluxRect const *ir, FluxColor color, float start_x, bool *ok
) {
	FluxMenu *m = dc->menu;
	if (!m->has_toggles) return start_x;
	FluxRect     check_box = {start_x, ir->y, 16.0f, ir->h};
	MenuTextDraw draw      = {NULL, &check_box, FLUX_MENU_ITEM_CHECK_GLYPH_SIZE, color, 1u, 1u, true};
	if (it->checked && it->type == FLUX_MENU_ITEM_TOGGLE) {
		draw.text = GLYPH_CHECKMARK;
		*ok       = *ok && rec_menu_text(dc, &draw);
	}
	if (it->checked && it->type == FLUX_MENU_ITEM_RADIO) {
		draw.text = GLYPH_RADIO_DOT;
		*ok       = *ok && rec_menu_text(dc, &draw);
	}
	return start_x + 28.0f;
}

static float menu_rec_icon_glyph(
  MenuRecContext const *dc, StoredItem const *it, FluxRect const *ir, FluxColor color, float start_x, bool *ok
) {
	FluxMenu *m = dc->menu;
	if (!m->has_icons) return start_x;
	if (!it->icon_glyph) return start_x + FLUX_MENU_ITEM_ICON_SIZE + FLUX_MENU_ITEM_ICON_GAP;
	FluxRect icon_box = {
	  start_x, ir->y + (ir->h - FLUX_MENU_ITEM_ICON_SIZE) * 0.5f, FLUX_MENU_ITEM_ICON_SIZE, FLUX_MENU_ITEM_ICON_SIZE};
	MenuTextDraw draw = {it->icon_glyph, &icon_box, FLUX_MENU_ITEM_ICON_SIZE, color, 1u, 1u, true};
	*ok               = *ok && rec_menu_text(dc, &draw);
	return start_x + FLUX_MENU_ITEM_ICON_SIZE + FLUX_MENU_ITEM_ICON_GAP;
}

static float menu_rec_trailing(
  MenuRecContext const *dc, StoredItem const *it, FluxRect const *ir, MenuItemPalette const *palette, bool *ok
) {
	FluxMenu *m           = dc->menu;
	float     label_right = ir->x + ir->w - FLUX_MENU_ITEM_PAD_RIGHT;
	if (it->type == FLUX_MENU_ITEM_SUBMENU) {
		FluxRect     chev_box = {label_right - FLUX_MENU_ITEM_CHEVRON_SIZE, ir->y, FLUX_MENU_ITEM_CHEVRON_SIZE, ir->h};
		MenuTextDraw draw
		  = {GLYPH_CHEVRON_R, &chev_box, FLUX_MENU_ITEM_CHEVRON_SIZE, palette->fg_chevron, 1u, 1u, true};
		*ok          = *ok && rec_menu_text(dc, &draw);
		label_right -= FLUX_MENU_ITEM_CHEVRON_SIZE + FLUX_MENU_ITEM_CHEVRON_GAP;
	}
	if (it->accelerator_text) {
		FluxRect     accel_box = {label_right - m->col_accel_w, ir->y, m->col_accel_w, ir->h};
		MenuTextDraw draw
		  = {it->accelerator_text, &accel_box, FLUX_MENU_ITEM_ACCEL_FONT_SIZE, palette->fg_accel, 2u, 1u, false};
		*ok          = *ok && rec_menu_text(dc, &draw);
		label_right -= m->col_accel_w + FLUX_MENU_ITEM_ACCEL_GAP;
	}
	return label_right;
}

static bool menu_rec_label_text(
  MenuRecContext const *dc, StoredItem const *it, FluxRect const *ir, FluxColor color, MenuLabelSpan const *span
) {
	if (!it->label) return true;
	float lw = span->label_right - span->text_x;
	if (lw < 0.0f) lw = 0.0f;
	FluxRect     lbl_box = {span->text_x, ir->y, lw, ir->h};
	MenuTextDraw draw    = {it->label, &lbl_box, FLUX_MENU_ITEM_FONT_SIZE, color, 0u, 1u, false};
	return rec_menu_text(dc, &draw);
}

static bool menu_rec_separator(MenuRecContext const *dc, StoredItem const *it) {
	float slot_top = dc->layout->content_y0 + it->y;
	float y_mid    = slot_top + FLUX_MENU_SEP_PAD_TOP + FLUX_MENU_SEP_HEIGHT * 0.5f;
	return xent_dl_strokeline(
	  dc->b, dc->layout->content_x0, y_mid, dc->layout->content_x0 + dc->layout->content_w, y_mid,
	  flux_rec_xcolor(dc->colors->separator), FLUX_MENU_SEP_HEIGHT, XENT_STROKE_CAP_FLAT
	);
}

static MenuItemState menu_item_state(FluxMenu *m, StoredItem const *it, int index) {
	return (MenuItemState) {
	  .pressed      = index == m->pressed_index,
	  .submenu_open = menu_item_submenu_open(m, it, index),
	  .hovered      = index == m->hovered_index,
	  .focused_kb   = index == m->keyboard_index,
	};
}

static bool menu_rec_item(MenuRecContext const *dc, int index) {
	FluxMenu       *m       = dc->menu;
	StoredItem     *it      = &m->items [index];
	MenuItemState   state   = menu_item_state(m, it, index);
	FluxRect        ir      = menu_item_rect(it, dc->layout);
	MenuItemPalette palette = menu_item_palette(dc->colors, it, &state);
	float           text_x  = ir.x + FLUX_MENU_ITEM_PAD_LEFT;
	bool            ok      = true;

	if (flux_color_af(palette.bg_fill) > 0.0f)
		ok = flux_rec_fill_rounded(dc->b, &ir, FLUX_MENU_ITEM_CORNER, palette.bg_fill);
	if (ok && state.focused_kb && it->enabled) ok = menu_rec_focus_ring(dc, &ir);
	text_x                    = menu_rec_toggle_marker(dc, it, &ir, palette.fg_text, text_x, &ok);
	text_x                    = menu_rec_icon_glyph(dc, it, &ir, palette.fg_text, text_x, &ok);
	float         label_right = menu_rec_trailing(dc, it, &ir, &palette, &ok);
	MenuLabelSpan span        = {text_x, label_right};
	return ok && menu_rec_label_text(dc, it, &ir, palette.fg_text, &span);
}

static XentDl *menu_record(
  FluxMenu *m, MenuPresenterLayout const *layout, ResolvedColors const *colors, FluxPopupMenuTransition const *tr
) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	MenuRecContext dc = {.menu = m, .b = b, .layout = layout, .colors = colors};
	bool           ok = true;

	if (tr->active) {
		XentAffine2 scale
		  = {1.0f, 0.0f, 0.0f, 0.0f, tr->border_scale_y, tr->border_center_y * (1.0f - tr->border_scale_y)};
		ok = xent_dl_pushtransform(b, scale);
	}
	if (ok) ok = menu_rec_presenter(&dc);
	if (tr->active) ok = xent_dl_poptransform(b) && ok;

	XentRect clip = {
	  layout->content_x0, layout->content_y0 + tr->clip_translate_y, layout->content_w,
	  layout->content_y1 - layout->content_y0};
	if (ok) ok = xent_dl_pushcliprect(b, clip);
	if (ok) ok = xent_dl_pushtransform(b, xent_affine_translate(0.0f, tr->content_translate_y - m->scroll_y));
	for (int i = 0; ok && i < m->item_count; i++) {
		StoredItem *it = &m->items [i];
		if (it->type == FLUX_MENU_ITEM_SEPARATOR) ok = menu_rec_separator(&dc, it);
		else ok = menu_rec_item(&dc, i);
	}
	if (ok) ok = xent_dl_poptransform(b);
	ok = xent_dl_popclip(b) && ok;
	return flux_rec_finish(b, ok);
}

void menu_paint(void *ctx, FluxPopup *popup) {
	FluxMenu *m = ( FluxMenu * ) ctx;
	if (!m) return;

	FluxGfx *gfx = flux_popup_gfx(popup);
	if (!gfx) return;
	ID2D1DeviceContext *d2d = flux_gfx_d2d(gfx);
	if (!d2d) return;

	ensure_brush(m, d2d);
	if (!m->brush) return;

	FluxThemeColors const *theme   = NULL;
	bool                   is_dark = false;
	menu_resolve_theme(m, &theme, &is_dark);

	ResolvedColors c               = resolve_colors(m);
	c.presenter_bg                 = flux_popup_acrylictint(popup, c.presenter_bg);
	FluxRenderCtx           rc     = menu_render_context(m, d2d, theme, is_dark);
	MenuPresenterLayout     layout = menu_presenter_layout(m);
	FluxPopupMenuTransition tr     = flux_popup_menutransition(popup, layout.bounds.h);
	XentDl                 *list   = menu_record(m, &layout, &c, &tr);
	if (list) {
		flux_xtk_replay_rc(&rc, list);
		xent_dl_free(list);
	}
}
