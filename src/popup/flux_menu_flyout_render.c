#include "flux_menu_internal.h"
#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_icon.h"
#include "render/flux_render_snapshot.h"

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
	FluxMenu             *menu;
	XentDlBuilder        *b;
	ResolvedColors const *colors;
} MenuRecContext;

typedef struct MenuTextDraw {
	char const     *text;
	FluxRect const *box;
	float           size;
	FluxColor       color;
	uint8_t         align;
	uint8_t         valign;
	bool            icon_font;
	bool            mirrorable;
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
	  .flags
	  = ( uint8_t ) ((draw->icon_font ? XENT_DL_TEXT_ICON : 0u) | (draw->mirrorable ? XENT_DL_TEXT_MIRRORABLE : 0u)),
	  .font_family = draw->icon_font ? flux_icon_font_family() : NULL,
	};
	return flux_rec_text_ex(dc->b, draw->box, draw->text, &style, draw->color);
}

static bool menu_item_submenu_open(FluxMenu *m, StoredItem const *it, int index) {
	if (it->type != FLUX_MENU_ITEM_SUBMENU || index != m->open_submenu_index) return false;
	return m->open_submenu && flux_popup_visible(m->open_submenu->popup);
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
		  = {GLYPH_CHEVRON_R, &chev_box, FLUX_MENU_ITEM_CHEVRON_SIZE, palette->fg_chevron, 1u, 1u, true, true};
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

static bool menu_rec_separator(MenuRecContext const *dc, FluxRect const *slot) {
	float y_mid = slot->y + FLUX_MENU_SEP_PAD_TOP + FLUX_MENU_SEP_HEIGHT * 0.5f;
	return xent_dl_strokeline(
	  dc->b, slot->x, y_mid, slot->x + slot->w, y_mid, flux_rec_xcolor(dc->colors->separator), FLUX_MENU_SEP_HEIGHT,
	  XENT_STROKE_CAP_FLAT
	);
}

static MenuItemState menu_item_state(FluxMenu *m, StoredItem const *it, int index, bool pressed) {
	return (MenuItemState) {
	  .pressed      = pressed,
	  .submenu_open = menu_item_submenu_open(m, it, index),
	  .hovered      = index == m->hovered_index,
	  .focused_kb   = index == m->keyboard_index,
	};
}

static bool menu_rec_item(MenuRecContext const *dc, int index, FluxRect const *slot, bool pressed) {
	FluxMenu     *m     = dc->menu;
	StoredItem   *it    = &m->items [index];
	MenuItemState state = menu_item_state(m, it, index, pressed);
	FluxRect      ir    = {
	  slot->x + FLUX_MENU_ITEM_MARGIN_H, slot->y + FLUX_MENU_ITEM_MARGIN_V, slot->w - 2.0f * FLUX_MENU_ITEM_MARGIN_H,
	  slot->h - 2.0f * FLUX_MENU_ITEM_MARGIN_V};
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

XentDl *flux_menurow_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) rc;
	FluxMenu *m     = ( FluxMenu * ) snap->u.menu_part.menu;
	int       index = snap->u.menu_part.index;
	if (!m || index < 0 || index >= m->item_count) return NULL;
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	ResolvedColors c  = resolve_colors(m);
	MenuRecContext dc = {.menu = m, .b = b, .colors = &c};
	bool           ok = m->items [index].type == FLUX_MENU_ITEM_SEPARATOR
	                    ? menu_rec_separator(&dc, bounds)
	                    : menu_rec_item(&dc, index, bounds, state && state->pressed);
	return flux_rec_finish(b, ok);
}

XentDl *flux_menupresenter_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) rc;
	( void ) state;
	FluxMenu *m = ( FluxMenu * ) snap->u.menu_part.menu;
	if (!m) return NULL;
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	ResolvedColors          c  = resolve_colors(m);
	FluxPopupMenuTransition tr = flux_popup_menutransition(m->popup, bounds->h);
	c.presenter_bg             = flux_popup_acrylictint(m->popup, c.presenter_bg);
	bool ok                    = true;
	if (tr.active) {
		float       cy    = bounds->y + tr.border_center_y;
		XentAffine2 scale = {1.0f, 0.0f, 0.0f, 0.0f, tr.border_scale_y, cy * (1.0f - tr.border_scale_y)};
		ok                = xent_dl_pushtransform(b, scale);
	}
	ok          = ok && flux_rec_fill_rounded(b, bounds, FLUX_MENU_PRESENTER_CORNER, c.presenter_bg);
	float    h  = FLUX_MENU_PRESENTER_BORDER * 0.5f;
	FluxRect in = {bounds->x + h, bounds->y + h, bounds->w - h * 2.0f, bounds->h - h * 2.0f};
	ok          = ok
	           && flux_rec_stroke_rounded(b, &in, FLUX_MENU_PRESENTER_CORNER, c.presenter_border, FLUX_MENU_PRESENTER_BORDER);
	if (tr.active) ok = xent_dl_poptransform(b) && ok;
	return flux_rec_finish(b, ok);
}
