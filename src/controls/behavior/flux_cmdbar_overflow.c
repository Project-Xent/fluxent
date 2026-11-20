#include "controls/behavior/flux_cmdbar_internal.h"
#include "controls/draw/flux_control_record_internal.h"
#include "controls/behavior/flux_flyout_binding.h"
#include "controls/factory/flux_factory.h"
#include "render/flux_icon.h"
#include "store/flux_node_data.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_text.h"
#include "fluxent/flux_theme.h"
#include "fluxent/flux_window.h"

#include <float.h>
#include <math.h>
#include <string.h>
#include <windows.h>

#define OVF_CHECK_GLYPH L"\xE73E"

typedef struct OvfMetrics {
	float pad_top;
	float pad_bot;
	float min_w;
} OvfMetrics;

typedef struct RowDraw {
	FluxThemeColors const    *t;
	FluxCmdBarData const     *b;
	FluxAppBarItemData const *d;
	FluxRect                  row;
	int                       index;
} RowDraw;

static FluxThemeColors const *ovf_theme(FluxCmdBarData const *b) {
	return b->theme ? flux_theme_colors(b->theme) : flux_theme_defaults();
}

static float ovf_scale(FluxCmdBarData const *b, RECT *client) {
	HWND hwnd = b->window ? flux_window_hwnd(b->window) : NULL;
	*client   = (RECT) {0};
	if (!hwnd) return 1.0f;
	GetClientRect(hwnd, client);
	UINT dpi = GetDpiForWindow(hwnd);
	return dpi ? ( float ) dpi / 96.0f : 1.0f;
}

static OvfMetrics ovf_metrics(FluxCmdBarData const *b) {
	if (b->touch)
		return (OvfMetrics) {FLUX_CMDBAR_OVF_TOUCH_TOP, FLUX_CMDBAR_OVF_TOUCH_BOT, FLUX_CMDBAR_OVF_TOUCH_MIN_W};
	return (OvfMetrics) {FLUX_CMDBAR_OVF_PAD_TOP, FLUX_CMDBAR_OVF_PAD_BOT, FLUX_CMDBAR_OVF_MIN_W};
}

static float ovf_text_w(FluxCmdBarData const *b, char const *s, float size) {
	if (!s || !s [0]) return 0.0f;
	if (!b->text) return 0.6f * size * ( float ) strlen(s);
	FluxTextStyle ts = {.font_size = size, .font_weight = FLUX_FONT_REGULAR};
	return ceilf(flux_text_measure(b->text, s, &ts, FLT_MAX).w);
}

static bool ovf_add(FluxCmdBarData *b, XentNodeId item, bool separator, float h) {
	if (b->nrows >= FLUX_CMDBAR_MAX_ROWS) return false;
	float y
	  = b->nrows ? b->rows [b->nrows - 1].y + b->rows [b->nrows - 1].h : FLUX_CMDBAR_OVF_BORDER + FLUX_CMDBAR_OVF_PAD_V;
	b->rows [b->nrows++] = (FluxCmdBarRow) {.item = item, .separator = separator, .y = y, .h = h};
	return true;
}

static float row_h(FluxCmdBarData const *b, bool separator) {
	OvfMetrics m = ovf_metrics(b);
	return separator ? FLUX_ABB_SEP_W + 2.0f * FLUX_CMDBAR_OVF_SEP_PAD : m.pad_top + FLUX_CMDBAR_OVF_LINE + m.pad_bot;
}

static void ovf_note(FluxCmdBarData *b, FluxAppBarItemData const *d) {
	if (d->kind == FLUX_APPBAR_SEPARATOR) return;
	b->with_icons   |= d->icon != NULL;
	b->with_toggles |= d->kind == FLUX_APPBAR_TOGGLE;
	b->accel_w       = fmaxf(b->accel_w, ovf_text_w(b, d->accel, FLUX_CMDBAR_OVF_ACCEL_SIZE));
}

static bool has_secondary(FluxCmdBarData const *b) {
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (d && d->secondary) return true;
	}
	return false;
}

static void build_rows(FluxCmdBarData *b) {
	int moved = 0;
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (!d || d->secondary || d->kind == FLUX_APPBAR_SEPARATOR || !flux_cmdbar_moved(b, c)) continue;
		moved += ovf_add(b, c, false, row_h(b, false));
		ovf_note(b, d);
	}
	if (moved && has_secondary(b)) ovf_add(b, XENT_NODE_INVALID, true, row_h(b, true));
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (!d || !d->secondary) continue;
		bool sep = d->kind == FLUX_APPBAR_SEPARATOR;
		ovf_add(b, c, sep, row_h(b, sep));
		ovf_note(b, d);
	}
}

static float label_indent(FluxCmdBarData const *b, FluxAppBarItemData const *d) {
	if (d->kind == FLUX_APPBAR_TOGGLE) return b->with_icons ? FLUX_CMDBAR_OVF_INDENT_TWO : FLUX_CMDBAR_OVF_INDENT_ONE;
	if (b->with_toggles && b->with_icons) return FLUX_CMDBAR_OVF_INDENT_TWO;
	return b->with_toggles || b->with_icons ? FLUX_CMDBAR_OVF_INDENT_ONE : FLUX_CMDBAR_OVF_INDENT;
}

static float icon_left(FluxCmdBarData const *b, FluxAppBarItemData const *d) {
	bool shifted = d->kind == FLUX_APPBAR_TOGGLE || b->with_toggles;
	return shifted ? FLUX_CMDBAR_OVF_INDENT_ONE : FLUX_CMDBAR_OVF_INDENT;
}

static float accel_part(FluxCmdBarData const *b) {
	return b->accel_w > 0.0f ? FLUX_CMDBAR_OVF_ACCEL_LEFT + b->accel_w + FLUX_CMDBAR_OVF_INDENT : 0.0f;
}

static float content_w(FluxCmdBarData const *b) {
	float w = 0.0f;
	for (int i = 0; i < b->nrows; i++) {
		FluxAppBarItemData *d = b->rows [i].separator ? NULL : flux_cmdbar_item(b->store, b->rows [i].item);
		if (!d) continue;
		float lw = label_indent(b, d) + ovf_text_w(b, d->label, FLUX_CMDBAR_OVF_LABEL_SIZE) + FLUX_CMDBAR_OVF_INDENT;
		w        = fmaxf(w, lw + accel_part(b));
	}
	return w + 2.0f * FLUX_CMDBAR_OVF_BORDER;
}

void flux_cmdbar_ovf_build(FluxCmdBarData *b) {
	b->nrows        = 0;
	b->with_icons   = false;
	b->with_toggles = false;
	b->accel_w      = 0.0f;
	b->scroll_y     = 0.0f;
	build_rows(b);
	RECT  client;
	float scale  = ovf_scale(b, &client);
	float view_w = ( float ) client.right / scale;
	float view_h = ( float ) client.bottom / scale;
	float min_w  = ovf_metrics(b).min_w;
	float max_w  = FLUX_CMDBAR_OVF_MAX_W;
	if (view_w > 0.0f && view_w <= FLUX_CMDBAR_OVF_MAX_W) min_w = max_w = view_w;
	b->ovf_w    = fminf(fmaxf(content_w(b), min_w), max_w);
	float last  = b->nrows ? b->rows [b->nrows - 1].y + b->rows [b->nrows - 1].h : 0.0f;
	b->ovf_h    = last + FLUX_CMDBAR_OVF_PAD_V + FLUX_CMDBAR_OVF_BORDER;
	b->ovf_view = b->ovf_h;
	if (view_h > 0.0f) b->ovf_view = fminf(b->ovf_h, view_h * 0.5f);
}

static float ovf_offset(FluxCmdBarData const *b, float bar_x, float view_w) {
	float offset = b->width - b->ovf_w;
	if (offset >= 0.0f || bar_x + offset >= 0.0f) return offset;
	return bar_x + b->ovf_w <= view_w ? 0.0f : -bar_x;
}

void flux_cmdbar_ovf_show(FluxCmdBarData *b) {
	if (!b->popup) return;
	FluxNodeData *nd = flux_store_find(b->store, b->more);
	b->touch         = nd && nd->state.pointer_type == FLUX_POINTER_TOUCH;
	flux_cmdbar_ovf_build(b);
	RECT     client;
	float    scale  = ovf_scale(b, &client);
	FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
	FluxRect local  = flux_binding_window_rect(b->ctx, b->store, b->node);
	float    w      = b->nrows ? b->ovf_w : 1.0f;
	float    h      = b->nrows ? b->ovf_view : 1.0f;
	float    dx     = ovf_offset(b, local.x, ( float ) client.right / scale);
	float    dy     = b->open_up ? b->closed_h - b->content_h - h : b->content_h;
	bool     rtl    = xent_resolved_dir(b->ctx, b->node) == XENT_DIRECTION_RTL;
	float    x      = rtl ? anchor.x + anchor.w - w * scale : anchor.x + dx * scale;
	flux_popup_setsize(b->popup, w, h);
	flux_popup_setdismissoutside(b->popup, !b->sticky);
	flux_popup_setrtl(b->popup, rtl);
	flux_popup_show(b->popup, (FluxRect) {x, anchor.y + dy * scale, 0.0f, 0.0f}, FLUX_PLACEMENT_BOTTOM);
}

static FluxColor row_fg(FluxThemeColors const *t, bool enabled, bool pressed) {
	if (!enabled) return t->text_disabled;
	return pressed ? t->text_secondary : t->text_primary;
}

static bool row_text(XentDlBuilder *db, FluxRect const *r, char const *s, float size, uint8_t align, FluxColor c) {
	XentTextStyle style = {.font_size = size, .font_weight = FLUX_FONT_REGULAR, .h_align = align, .v_align = 1u};
	return !s || flux_rec_text_ex(db, r, s, &style, c);
}

static bool row_icon(XentDlBuilder *db, RowDraw const *rd, FluxColor c) {
	if (!rd->b->with_icons || !rd->d->icon) return true;
	FluxRect       r     = {rd->row.x + icon_left(rd->b, rd->d), rd->row.y, FLUX_ABB_ICON, rd->row.h};
	XentTextStyle  style = {.font_size = FLUX_ABB_ICON, .h_align = 1u, .v_align = 1u, .flags = XENT_DL_TEXT_ICON};
	char           utf8 [8];
	wchar_t const *wc = flux_icon_lookup(rd->d->icon);
	if (wc && flux_icon_to_utf8(wc, utf8, sizeof(utf8)) > 0) return flux_rec_text_ex(db, &r, utf8, &style, c);
	return flux_rec_text_ex(db, &r, rd->d->icon, &style, c);
}

static bool row_check(XentDlBuilder *db, RowDraw const *rd, FluxColor c) {
	if (rd->d->kind != FLUX_APPBAR_TOGGLE || !rd->d->checked) return true;
	float    y = rd->row.y + (rd->row.h - FLUX_CMDBAR_OVF_CHECK) * 0.5f;
	FluxRect r = {rd->row.x + FLUX_CMDBAR_OVF_CHECK_LEFT, y, FLUX_CMDBAR_OVF_CHECK, FLUX_CMDBAR_OVF_CHECK};
	return flux_rec_glyph_wc(db, &r, OVF_CHECK_GLYPH, FLUX_CMDBAR_OVF_CHECK_GLYPH, c);
}

static bool row_chrome(XentDlBuilder *db, RowDraw const *rd, FluxRect const *inner) {
	bool hot     = rd->index == rd->b->hover && rd->d->enabled;
	bool pressed = hot && rd->index == rd->b->pressed;
	bool ok      = true;
	if (hot)
		ok = flux_rec_fill_rounded(
		  db, inner, FLUX_ABB_CORNER, pressed ? rd->t->subtle_fill_tertiary : rd->t->subtle_fill_secondary
		);
	if (ok && rd->index == rd->b->focus_row) ok = flux_rec_focus_rect(db, rd->t, inner, FLUX_ABB_CORNER);
	return ok;
}

static bool draw_item(XentDlBuilder *db, RowDraw const *rd) {
	FluxRect inner
	  = {rd->row.x + FLUX_CMDBAR_OVF_ITEM_MARGIN, rd->row.y, rd->row.w - 2.0f * FLUX_CMDBAR_OVF_ITEM_MARGIN, rd->row.h};
	bool      pressed = rd->index == rd->b->pressed && rd->index == rd->b->hover;
	FluxColor fg      = row_fg(rd->t, rd->d->enabled, pressed);
	float     indent  = label_indent(rd->b, rd->d);
	float     accel   = accel_part(rd->b);
	FluxRect  label   = {rd->row.x + indent, rd->row.y, rd->row.w - indent - FLUX_CMDBAR_OVF_INDENT - accel, rd->row.h};
	FluxRect  keys
	  = {rd->row.x + rd->row.w - FLUX_CMDBAR_OVF_INDENT - rd->b->accel_w, rd->row.y, rd->b->accel_w, rd->row.h};
	FluxColor kc = rd->d->enabled ? rd->t->text_secondary : rd->t->text_disabled;
	return row_chrome(db, rd, &inner)
	    && row_check(db, rd, fg)
	    && row_icon(db, rd, fg)
	    && row_text(db, &label, rd->d->label, FLUX_CMDBAR_OVF_LABEL_SIZE, 0u, fg)
	    && row_text(db, &keys, rd->d->accel, FLUX_CMDBAR_OVF_ACCEL_SIZE, 2u, kc);
}

static bool draw_row(XentDlBuilder *db, FluxCmdBarData const *b, FluxThemeColors const *t, int i) {
	FluxCmdBarRow const *row = &b->rows [i];
	float                x   = FLUX_CMDBAR_OVF_BORDER;
	float                w   = b->ovf_w - 2.0f * FLUX_CMDBAR_OVF_BORDER;
	if (row->separator) {
		FluxRect line = {x, row->y + FLUX_CMDBAR_OVF_SEP_PAD, w, FLUX_ABB_SEP_W};
		return flux_rec_fill_rect(db, &line, t->divider_stroke_default);
	}
	FluxAppBarItemData const *d = flux_cmdbar_item(b->store, row->item);
	if (!d) return true;
	RowDraw rd = {
	  .t = t, .b = b, .d = d, .row = {x, row->y, w, row->h},
                 .index = i
    };
	return draw_item(db, &rd);
}

static bool draw_frame(XentDlBuilder *db, FluxCmdBarData const *b, FluxThemeColors const *t, FluxPopup const *popup) {
	FluxRect  all   = {0.0f, 0.0f, b->ovf_w, b->ovf_view};
	float     half  = FLUX_CMDBAR_OVF_BORDER * 0.5f;
	FluxRect  inset = {half, half, b->ovf_w - FLUX_CMDBAR_OVF_BORDER, b->ovf_view - FLUX_CMDBAR_OVF_BORDER};
	FluxColor bg    = flux_popup_acrylictint(popup, t->flyout_background);
	return flux_rec_fill_rounded(db, &all, FLUX_CMDBAR_OVF_CORNER, bg)
	    && flux_rec_stroke_rounded(db, &inset, FLUX_CMDBAR_OVF_CORNER, t->flyout_border, FLUX_CMDBAR_OVF_BORDER);
}

static bool draw_rows(XentDlBuilder *db, FluxCmdBarData const *b, FluxThemeColors const *t) {
	FluxRect view = {
	  FLUX_CMDBAR_OVF_BORDER, FLUX_CMDBAR_OVF_BORDER, b->ovf_w - 2.0f * FLUX_CMDBAR_OVF_BORDER,
	  b->ovf_view - 2.0f * FLUX_CMDBAR_OVF_BORDER};
	bool ok = xent_dl_pushcliprect(db, flux_rec_xrect(&view))
	       && xent_dl_pushtransform(db, xent_affine_translate(0.0f, -b->scroll_y));
	for (int i = 0; ok && i < b->nrows; i++) ok = draw_row(db, b, t, i);
	return ok && xent_dl_poptransform(db) && xent_dl_popclip(db);
}

static XentDl *ovf_record(void *ctx, FluxPopup const *popup) {
	FluxCmdBarData *b  = ( FluxCmdBarData * ) ctx;
	XentDlBuilder  *db = xent_dl_begin();
	if (!db) return NULL;
	if (!b->nrows) return flux_rec_finish(db, true);
	FluxThemeColors const *t     = ovf_theme(b);
	float                  slide = (1.0f - b->reveal) * b->ovf_view * (b->open_up ? 1.0f : -1.0f);
	FluxRect               all   = {0.0f, 0.0f, b->ovf_w, b->ovf_view};
	bool                   ok    = xent_dl_pushcliprect(db, flux_rec_xrect(&all))
	                            && xent_dl_pushtransform(db, xent_affine_translate(0.0f, slide))
	                            && draw_frame(db, b, t, popup)
	                            && draw_rows(db, b, t)
	                            && xent_dl_poptransform(db)
	                            && xent_dl_popclip(db);
	return flux_rec_finish(db, ok);
}

static int row_at(FluxCmdBarData const *b, float y) {
	y += b->scroll_y;
	for (int i = 0; i < b->nrows; i++)
		if (!b->rows [i].separator && y >= b->rows [i].y && y < b->rows [i].y + b->rows [i].h) return i;
	return -1;
}

static void ovf_activate(FluxCmdBarData *b, int i) {
	FluxAppBarItemData *d = i >= 0 && i < b->nrows ? flux_cmdbar_item(b->store, b->rows [i].item) : NULL;
	if (d && !b->rows [i].separator) flux_cmdbar_execute(d);
}

static void ovf_scroll(FluxCmdBarData *b, float dy) {
	float max   = b->ovf_h - b->ovf_view;
	b->scroll_y = fminf(fmaxf(b->scroll_y - dy, 0.0f), fmaxf(max, 0.0f));
}

static void ovf_release(FluxCmdBarData *b, int i) {
	int pressed = b->pressed;
	b->pressed  = -1;
	if (i >= 0 && i == pressed) ovf_activate(b, i);
}

static void ovf_mouse(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y) {
	( void ) popup;
	( void ) x;
	FluxCmdBarData *b = ( FluxCmdBarData * ) ctx;
	int             i = row_at(b, y);
	switch (event) {
	case FLUX_POPUP_MOUSE_MOVE : b->hover = i; break;
	case FLUX_POPUP_MOUSE_DOWN : b->pressed = i; break;
	case FLUX_POPUP_MOUSE_UP   : ovf_release(b, i); break;
	case FLUX_POPUP_MOUSE_LEAVE :
		b->hover   = -1;
		b->pressed = -1;
		break;
	case FLUX_POPUP_MOUSE_WHEEL : ovf_scroll(b, y); break;
	default                     : break;
	}
	flux_cmdbar_repaint(b);
}

static void ovf_dismissed(void *ctx) {
	FluxCmdBarData *b = ( FluxCmdBarData * ) ctx;
	if (b->open) flux_cmdbar_open(b, false, true, false);
}

int flux_cmdbar_ovf_edge(FluxCmdBarData const *b, bool last) {
	for (int k = 0; k < b->nrows; k++) {
		int i = last ? b->nrows - 1 - k : k;
		if (!b->rows [i].separator) return i;
	}
	return -1;
}

static int ovf_step(FluxCmdBarData const *b, int from, int dir) {
	for (int i = from + dir; i >= 0 && i < b->nrows; i += dir)
		if (!b->rows [i].separator) return i;
	return -1;
}

static void ovf_reveal(FluxCmdBarData *b, int i) {
	if (i < 0) return;
	float top    = b->rows [i].y - FLUX_CMDBAR_OVF_BORDER - FLUX_CMDBAR_OVF_PAD_V;
	float bottom = b->rows [i].y + b->rows [i].h + FLUX_CMDBAR_OVF_PAD_V + FLUX_CMDBAR_OVF_BORDER - b->ovf_view;
	if (top < b->scroll_y) b->scroll_y = fmaxf(0.0f, top);
	if (bottom > b->scroll_y) b->scroll_y = bottom;
}

bool flux_cmdbar_ovf_key(FluxCmdBarData *b, FluxKey key) {
	if (!b->nrows) return false;
	bool vertical = key == FLUX_KEY_UP || key == FLUX_KEY_DOWN;
	if (vertical) {
		int dir      = key == FLUX_KEY_DOWN ? 1 : -1;
		b->focus_row = b->focus_row < 0 ? flux_cmdbar_ovf_edge(b, dir < 0) : ovf_step(b, b->focus_row, dir);
		ovf_reveal(b, b->focus_row);
		flux_cmdbar_repaint(b);
		return true;
	}
	if ((key == FLUX_KEY_ENTER || key == FLUX_KEY_SPACE) && b->focus_row >= 0) {
		ovf_activate(b, b->focus_row);
		return true;
	}
	if (key == FLUX_KEY_TAB && b->focus_row >= 0) flux_cmdbar_open(b, false, true, false);
	return false;
}

bool flux_cmdbar_ovf_create(FluxCmdBarData *b) {
	if (!b->window) return true;
	b->popup = flux_popup_create(b->window);
	if (!b->popup) return false;
	flux_popup_setanim(b->popup, FLUX_POPUP_ANIM_NONE);
	flux_popup_setshadow(b->popup, false, FLUX_CMDBAR_OVF_CORNER);
	flux_popup_setdismissoutside(b->popup, true);
	flux_popup_onrecord(b->popup, ovf_record, b, b->text);
	flux_popup_onmouse(b->popup, ovf_mouse, b);
	flux_popup_ondismiss(b->popup, ovf_dismissed, b);
	return true;
}

void flux_cmdbar_ovf_destroy(FluxCmdBarData *b) {
	if (b->popup) flux_popup_destroy(b->popup);
	b->popup = NULL;
}
