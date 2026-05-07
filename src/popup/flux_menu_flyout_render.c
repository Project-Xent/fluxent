#include "flux_menu_flyout_internal.h"
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

typedef struct MenuDrawContext {
	FluxMenuFlyout            *menu;
	FluxRenderContext const   *rc;
	ID2D1RenderTarget         *rt;
	MenuPresenterLayout const *layout;
	ResolvedColors const      *colors;
} MenuDrawContext;

typedef struct MenuTextDraw {
	char const     *text;
	FluxRect const *box;
	float           size;
	FluxColor       color;
	FluxTextAlign   align;
	FluxTextVAlign  valign;
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

static void ensure_brush(FluxMenuFlyout *m, ID2D1DeviceContext *d2d) {
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

static void resolve_presenter_colors(ResolvedColors *r, FluxMenuFlyout const *m, bool dark) {
	bool have_backdrop  = m->popup && flux_popup_has_system_backdrop(m->popup);
	r->presenter_bg     = have_backdrop
	                      ? (dark ? flux_color_rgba(0x2c, 0x2c, 0x2c, 0x7a) : flux_color_rgba(0xfc, 0xfc, 0xfc, 0x66))
	                      : (dark ? flux_color_rgb(0x2c, 0x2c, 0x2c) : flux_color_rgb(0xf9, 0xf9, 0xf9));
	r->presenter_border = dark ? flux_color_rgba(0, 0, 0, 0x33) : flux_color_rgba(0, 0, 0, 0x0f);
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

static ResolvedColors resolve_colors(FluxMenuFlyout const *m) {
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

static void draw_menu_text(MenuDrawContext const *dc, MenuTextDraw const *draw) {
	if (!dc->menu->text || !draw->text || !*draw->text) return;
	FluxTextStyle ts = {0};
	ts.font_family   = draw->icon_font ? "Segoe Fluent Icons" : NULL;
	ts.font_size     = draw->size;
	ts.font_weight   = FLUX_FONT_REGULAR;
	ts.text_align    = draw->align;
	ts.vert_align    = draw->valign;
	ts.color         = draw->color;
	flux_text_draw(dc->menu->text, dc->rt, draw->text, draw->box, &ts);
}

static void draw_glyph(MenuDrawContext const *dc, MenuTextDraw const *draw) {
	MenuTextDraw glyph = *draw;
	glyph.icon_font    = true;
	draw_menu_text(dc, &glyph);
}

static void draw_label(MenuDrawContext const *dc, MenuTextDraw const *draw) {
	MenuTextDraw label = *draw;
	label.valign       = FLUX_TEXT_VCENTER;
	label.icon_font    = false;
	draw_menu_text(dc, &label);
}

static FluxRenderContext
menu_render_context(FluxMenuFlyout *m, ID2D1DeviceContext *d2d, FluxThemeColors const *theme, bool is_dark) {
	FluxRenderContext rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d     = d2d;
	rc.brush   = m->brush;
	rc.text    = m->text;
	rc.theme   = theme;
	rc.dpi     = flux_window_dpi(m->owner);
	rc.is_dark = is_dark;
	return rc;
}

static MenuPresenterLayout menu_presenter_layout(FluxMenuFlyout *m) {
	MenuPresenterLayout layout;
	layout.bounds     = (FluxRect) {0.0f, 0.0f, m->total_w, m->total_h};
	layout.content_x0 = FLUX_MENU_PRESENTER_BORDER;
	layout.content_y0 = FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_PRESENTER_PAD_TOP;
	layout.content_w  = m->total_w - 2.0f * FLUX_MENU_PRESENTER_BORDER;
	layout.content_y1 = m->total_h - FLUX_MENU_PRESENTER_BORDER - FLUX_MENU_PRESENTER_PAD_BOTTOM;
	return layout;
}

static void
menu_draw_presenter(FluxRenderContext const *rc, MenuPresenterLayout const *layout, ResolvedColors const *colors) {
	flux_fill_rounded_rect(rc, &layout->bounds, FLUX_MENU_PRESENTER_CORNER, colors->presenter_bg);

	float    half  = FLUX_MENU_PRESENTER_BORDER * 0.5f;
	FluxRect inset = {
	  layout->bounds.x + half, layout->bounds.y + half, layout->bounds.w - half * 2.0f, layout->bounds.h - half * 2.0f};
	flux_draw_rounded_rect(
	  rc, &inset, FLUX_MENU_PRESENTER_CORNER, colors->presenter_border, FLUX_MENU_PRESENTER_BORDER
	);
}

static void menu_begin_scroll_clip(
  FluxMenuFlyout *m, ID2D1RenderTarget *rt, MenuPresenterLayout const *layout, D2D1_MATRIX_3X2_F *prev_xform
) {
	D2D1_RECT_F clip_rect
	  = {layout->content_x0, layout->content_y0, layout->content_x0 + layout->content_w, layout->content_y1};
	ID2D1RenderTarget_PushAxisAlignedClip(rt, &clip_rect, D2D1_ANTIALIAS_MODE_ALIASED);
	ID2D1RenderTarget_GetTransform(rt, prev_xform);

	D2D1_MATRIX_3X2_F scroll_xform  = *prev_xform;
	scroll_xform._32               -= m->scroll_y;
	ID2D1RenderTarget_SetTransform(rt, &scroll_xform);
}

static void menu_end_scroll_clip(ID2D1RenderTarget *rt, D2D1_MATRIX_3X2_F const *prev_xform) {
	ID2D1RenderTarget_SetTransform(rt, prev_xform);
	ID2D1RenderTarget_PopAxisAlignedClip(rt);
}

static bool menu_item_submenu_open(FluxMenuFlyout *m, StoredItem const *it, int index) {
	if (it->type != FLUX_MENU_ITEM_SUBMENU || index != m->open_submenu_index) return false;
	return m->open_submenu && flux_popup_is_visible(m->open_submenu->popup);
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

static void menu_draw_focus_ring(FluxRenderContext const *rc, FluxRect const *ir, ResolvedColors const *colors) {
	float const outer_w    = 2.0f;
	float const inner_w    = 1.0f;
	float       oh         = outer_w * 0.5f;
	FluxRect    outer_rect = {ir->x + oh, ir->y + oh, ir->w - outer_w, ir->h - outer_w};
	flux_draw_rounded_rect(rc, &outer_rect, FLUX_MENU_ITEM_CORNER, colors->focus_outer, outer_w);

	float    ins        = outer_w + inner_w * 0.5f;
	FluxRect inner_rect = {ir->x + ins, ir->y + ins, ir->w - 2.0f * ins, ir->h - 2.0f * ins};
	float    inner_r    = FLUX_MENU_ITEM_CORNER - outer_w;
	if (inner_r < 0.0f) inner_r = 0.0f;
	flux_draw_rounded_rect(rc, &inner_rect, inner_r, colors->focus_inner, inner_w);
}

static float menu_draw_toggle_marker(
  MenuDrawContext const *dc, StoredItem const *it, FluxRect const *ir, FluxColor color, float start_x
) {
	FluxMenuFlyout *m = dc->menu;
	if (!m->has_toggles) return start_x;

	FluxRect     check_box = {start_x, ir->y, 16.0f, ir->h};
	MenuTextDraw draw
	  = {NULL, &check_box, FLUX_MENU_ITEM_CHECK_GLYPH_SIZE, color, FLUX_TEXT_CENTER, FLUX_TEXT_VCENTER, true};
	if (it->checked && it->type == FLUX_MENU_ITEM_TOGGLE) {
		draw.text = GLYPH_CHECKMARK;
		draw_glyph(dc, &draw);
	}
	if (it->checked && it->type == FLUX_MENU_ITEM_RADIO) {
		draw.text = GLYPH_RADIO_DOT;
		draw_glyph(dc, &draw);
	}
	return start_x + 28.0f;
}

static float menu_draw_icon_glyph(
  MenuDrawContext const *dc, StoredItem const *it, FluxRect const *ir, FluxColor color, float start_x
) {
	FluxMenuFlyout *m = dc->menu;
	if (!m->has_icons) return start_x;
	if (!it->icon_glyph) return start_x + FLUX_MENU_ITEM_ICON_SIZE + FLUX_MENU_ITEM_ICON_GAP;

	FluxRect icon_box = {
	  start_x, ir->y + (ir->h - FLUX_MENU_ITEM_ICON_SIZE) * 0.5f, FLUX_MENU_ITEM_ICON_SIZE, FLUX_MENU_ITEM_ICON_SIZE};
	MenuTextDraw draw
	  = {it->icon_glyph, &icon_box, FLUX_MENU_ITEM_ICON_SIZE, color, FLUX_TEXT_CENTER, FLUX_TEXT_VCENTER, true};
	draw_glyph(dc, &draw);
	return start_x + FLUX_MENU_ITEM_ICON_SIZE + FLUX_MENU_ITEM_ICON_GAP;
}

static float menu_draw_trailing(
  MenuDrawContext const *dc, StoredItem const *it, FluxRect const *ir, MenuItemPalette const *palette
) {
	FluxMenuFlyout *m           = dc->menu;
	float           label_right = ir->x + ir->w - FLUX_MENU_ITEM_PAD_RIGHT;

	if (it->type == FLUX_MENU_ITEM_SUBMENU) {
		FluxRect     chev_box = {label_right - FLUX_MENU_ITEM_CHEVRON_SIZE, ir->y, FLUX_MENU_ITEM_CHEVRON_SIZE, ir->h};
		MenuTextDraw draw     = {
		  GLYPH_CHEVRON_R, &chev_box, FLUX_MENU_ITEM_CHEVRON_SIZE, palette->fg_chevron, FLUX_TEXT_CENTER,
		  FLUX_TEXT_VCENTER, true};
		draw_glyph(dc, &draw);
		label_right -= FLUX_MENU_ITEM_CHEVRON_SIZE + FLUX_MENU_ITEM_CHEVRON_GAP;
	}
	if (it->accelerator_text) {
		FluxRect     accel_box = {label_right - m->col_accel_w, ir->y, m->col_accel_w, ir->h};
		MenuTextDraw draw      = {
		  it->accelerator_text, &accel_box, FLUX_MENU_ITEM_ACCEL_FONT_SIZE, palette->fg_accel, FLUX_TEXT_RIGHT,
		  FLUX_TEXT_VCENTER, false};
		draw_label(dc, &draw);
		label_right -= m->col_accel_w + FLUX_MENU_ITEM_ACCEL_GAP;
	}
	return label_right;
}

static void menu_draw_label_text(
  MenuDrawContext const *dc, StoredItem const *it, FluxRect const *ir, FluxColor color, MenuLabelSpan const *span
) {
	if (!it->label) return;

	float lw = span->label_right - span->text_x;
	if (lw < 0.0f) lw = 0.0f;
	FluxRect     lbl_box = {span->text_x, ir->y, lw, ir->h};
	MenuTextDraw draw
	  = {it->label, &lbl_box, FLUX_MENU_ITEM_FONT_SIZE, color, FLUX_TEXT_LEFT, FLUX_TEXT_VCENTER, false};
	draw_label(dc, &draw);
}

static void menu_draw_separator(MenuDrawContext const *dc, StoredItem const *it) {
	float slot_top = dc->layout->content_y0 + it->y;
	float y_mid    = slot_top + FLUX_MENU_SEP_PAD_TOP + FLUX_MENU_SEP_HEIGHT * 0.5f;
	flux_draw_line(
	  dc->rc, &(FluxLineSpec) {dc->layout->content_x0, y_mid, dc->layout->content_x0 + dc->layout->content_w, y_mid},
	  dc->colors->separator, FLUX_MENU_SEP_HEIGHT
	);
}

static MenuItemState menu_item_state(FluxMenuFlyout *m, StoredItem const *it, int index) {
	return (MenuItemState) {
	  .pressed      = index == m->pressed_index,
	  .submenu_open = menu_item_submenu_open(m, it, index),
	  .hovered      = index == m->hovered_index,
	  .focused_kb   = index == m->keyboard_index,
	};
}

static void menu_draw_item(MenuDrawContext const *dc, int index) {
	FluxMenuFlyout *m       = dc->menu;
	StoredItem     *it      = &m->items [index];
	MenuItemState   state   = menu_item_state(m, it, index);
	FluxRect        ir      = menu_item_rect(it, dc->layout);
	MenuItemPalette palette = menu_item_palette(dc->colors, it, &state);
	float           text_x  = ir.x + FLUX_MENU_ITEM_PAD_LEFT;

	if (flux_color_af(palette.bg_fill) > 0.0f)
		flux_fill_rounded_rect(dc->rc, &ir, FLUX_MENU_ITEM_CORNER, palette.bg_fill);
	if (state.focused_kb && it->enabled) menu_draw_focus_ring(dc->rc, &ir, dc->colors);

	text_x                    = menu_draw_toggle_marker(dc, it, &ir, palette.fg_text, text_x);
	text_x                    = menu_draw_icon_glyph(dc, it, &ir, palette.fg_text, text_x);
	float         label_right = menu_draw_trailing(dc, it, &ir, &palette);
	MenuLabelSpan span        = {text_x, label_right};
	menu_draw_label_text(dc, it, &ir, palette.fg_text, &span);
}

void menu_paint(void *ctx, FluxPopup *popup) {
	FluxMenuFlyout *m = ( FluxMenuFlyout * ) ctx;
	if (!m) return;

	FluxGraphics *gfx = flux_popup_get_graphics(popup);
	if (!gfx) return;
	ID2D1DeviceContext *d2d = flux_graphics_get_d2d_context(gfx);
	if (!d2d) return;

	ensure_brush(m, d2d);
	if (!m->brush) return;

	FluxThemeColors const *theme   = NULL;
	bool                   is_dark = false;
	menu_resolve_theme(m, &theme, &is_dark);

	ResolvedColors      c      = resolve_colors(m);
	FluxRenderContext   rc     = menu_render_context(m, d2d, theme, is_dark);
	ID2D1RenderTarget  *rt     = ( ID2D1RenderTarget * ) d2d;
	MenuPresenterLayout layout = menu_presenter_layout(m);
	MenuDrawContext     dc     = {.menu = m, .rc = &rc, .rt = rt, .layout = &layout, .colors = &c};
	D2D1_MATRIX_3X2_F   prev_xform;

	menu_draw_presenter(&rc, &layout, &c);
	menu_begin_scroll_clip(m, rt, &layout, &prev_xform);
	for (int i = 0; i < m->item_count; i++) {
		StoredItem *it = &m->items [i];
		if (it->type == FLUX_MENU_ITEM_SEPARATOR) menu_draw_separator(&dc, it);
		else menu_draw_item(&dc, i);
	}
	menu_end_scroll_clip(rt, &prev_xform);
}
