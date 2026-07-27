#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "bridge/flux_internal.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_fluent.h"
#include "runtime/flux_str.h"

#include "controls/factory/flux_init_popup.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_window.h"

#include <math.h>

#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* WinUI ComboBox drop-down (ComboBox_themeresources.xaml): item pitch driven by
 * ComboBoxItemThemePadding 11,5,11,7 with 5,2 margins and corner 3; selected items
 * show a 3x16 accent pill; OverlayCornerRadius 8; MaxDropDownHeight 504. */
#define CB_ROW_H            36.0f /* ComboBoxItemThemePadding 11,5,11,7 + 2,2 margin. */
#define CB_ROW_H_TOUCH      48.0f /* ComboBoxItemThemeTouchPadding 11,11,11,13 + margin. */
#define CB_ITEM_MARGIN_X    5.0f
#define CB_ITEM_MARGIN_Y    2.0f
#define CB_ITEM_CORNER      3.0f
#define CB_ITEM_PAD_LEFT    11.0f
#define CB_CONTENT_MARGIN_Y 4.0f
#define CB_PILL_W           3.0f
#define CB_PILL_H           16.0f
#define CB_PILL_CORNER      1.5f
#define CB_MAX_HEIGHT       504.0f
#define CB_CORNER           8.0f
#define CB_BORDER           1.0f

/* The runtime embeds the model as its first member so component_data can point at
 * the model while the runtime owns the popup. */
typedef struct FluxComboRuntime {
	FluxComboBoxData model;
	FluxStore       *store;
	XentNodeId       node;
	XentCtx         *ctx;
	FluxWindow      *window;
	FluxText        *text;
	FluxTheme       *theme;
	FluxPopup       *popup;
	int              highlight;
	int              pressed_index; /**< Item the press landed on; -1 when none. */
	float            width;
	float            height;
	float            scroll_y;         /**< Drop-down scroll offset when items exceed 504px. */
	float            content_h;        /**< Total item-block height. */
	float            row_h;            /**< Per-item height; grows under touch input. */
	bool             panning;          /**< Touch drag exceeded the slop: scroll, don't select. */
	float            press_y;          /**< Pointer y at the press, for the pan slop test. */
	float            pan_start_scroll; /**< scroll_y at the press; the pan offsets from here. */

	wchar_t          search [64];      /**< IsTextSearchEnabled typeahead buffer. */
	int              search_len;
	DWORD            search_last;      /**< Tick of the last search char (1 s reset). */
} FluxComboRuntime;

/* WinUI lists treat a touch contact as a tap only until it travels past the
 * DirectManipulation slop; beyond it the gesture becomes a pan. */
#define CB_TOUCH_PAN_SLOP 10.0f

static void combo_resolve_theme(FluxComboRuntime *rt, FluxThemeColors const **theme, bool *is_dark) {
	if (rt->theme) {
		*theme          = flux_theme_colors(rt->theme);
		FluxThemeMode m = flux_theme_mode(rt->theme);
		*is_dark        = (m == FLUX_THEME_DARK) || (m == FLUX_THEME_SYSTEM && flux_theme_systemdark());
		return;
	}
	*theme   = flux_theme_defaults();
	*is_dark = false;
}

static float combo_dropdown_height(int count, float row_h) {
	float h = 2.0f * CB_BORDER + 2.0f * CB_CONTENT_MARGIN_Y + ( float ) count * row_h;
	return flux_minf(h, CB_MAX_HEIGHT);
}

static float combo_viewport_h(FluxComboRuntime const *rt) {
	return flux_maxf(0.0f, rt->height - 2.0f * CB_BORDER - 2.0f * CB_CONTENT_MARGIN_Y);
}

static float combo_max_scroll(FluxComboRuntime const *rt) {
	return flux_maxf(0.0f, rt->content_h - combo_viewport_h(rt));
}

static void combo_clamp_scroll(FluxComboRuntime *rt) {
	rt->scroll_y = flux_clampf(rt->scroll_y, 0.0f, combo_max_scroll(rt));
}

static void combo_ensure_visible(FluxComboRuntime *rt, int i) {
	if (i < 0) return;
	float top = ( float ) i * rt->row_h;
	float vp  = combo_viewport_h(rt);
	if (top < rt->scroll_y) rt->scroll_y = top;
	else if (top + rt->row_h > rt->scroll_y + vp) rt->scroll_y = top + rt->row_h - vp;
	combo_clamp_scroll(rt);
}

static int combo_item_at(FluxComboRuntime const *rt, float y) {
	float start = CB_BORDER + CB_CONTENT_MARGIN_Y;
	if (y < start || y > start + combo_viewport_h(rt)) return -1;
	int i = ( int ) ((y - start + rt->scroll_y) / rt->row_h);
	return (i >= 0 && i < rt->model.item_count) ? i : -1;
}

/* Row backplate per state; alpha 0 = none (ComboBoxItem CommonStates). */
static FluxColor combo_item_fill(FluxThemeColors const *t, bool selected, bool hot, bool pressed) {
	if (pressed) return t->subtle_fill_tertiary;
	if (hot) return selected ? t->subtle_fill_tertiary : t->subtle_fill_secondary;
	if (selected) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

static bool combo_rec_item_label(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *item, char const *label) {
	if (!label || !label [0]) return true;
	FluxRect tr = {item->x + CB_ITEM_PAD_LEFT, item->y, item->w - CB_ITEM_PAD_LEFT - 4.0f, item->h};
	if (tr.w <= 0.0f) return true;
	XentTextStyle style
	  = {.font_size = FLUX_FONT_SIZE_DEFAULT, .font_weight = FLUX_FONT_REGULAR, .h_align = 0u, .v_align = 1u};
	return flux_rec_text_ex(b, &tr, label, &style, t->text_primary);
}

static bool combo_rec_item(XentDlBuilder *b, FluxComboRuntime const *rt, FluxThemeColors const *t, int i) {
	float    row  = CB_BORDER + CB_CONTENT_MARGIN_Y + ( float ) i * rt->row_h - rt->scroll_y;
	FluxRect item = {
	  CB_BORDER + CB_ITEM_MARGIN_X, row + CB_ITEM_MARGIN_Y, rt->width - 2.0f * (CB_BORDER + CB_ITEM_MARGIN_X),
	  rt->row_h - 2.0f * CB_ITEM_MARGIN_Y};

	bool      selected = i == rt->model.selected_index;
	FluxColor fill     = combo_item_fill(t, selected, i == rt->highlight, i == rt->pressed_index);
	bool      ok       = true;
	if (flux_color_af(fill) > 0.0f) ok = flux_rec_fill_rounded(b, &item, CB_ITEM_CORNER, fill);
	if (ok && selected) {
		FluxRect pill = {item.x + 1.0f, item.y + (item.h - CB_PILL_H) * 0.5f, CB_PILL_W, CB_PILL_H};
		ok            = flux_rec_fill_rounded(b, &pill, CB_PILL_CORNER, t->accent_default);
	}
	return ok && combo_rec_item_label(b, t, &item, rt->model.items [i]);
}

static bool combo_rec_scrollbar(XentDlBuilder *b, FluxComboRuntime const *rt, FluxThemeColors const *t, float vp) {
	float max_scroll = combo_max_scroll(rt);
	if (max_scroll <= 0.0f) return true;
	float    track_top = CB_BORDER + CB_CONTENT_MARGIN_Y;
	float    thumb_h   = flux_maxf(20.0f, vp * vp / rt->content_h);
	float    thumb_y   = track_top + (rt->scroll_y / max_scroll) * (vp - thumb_h);
	FluxRect thumb     = {rt->width - CB_BORDER - 5.0f, thumb_y, 3.0f, thumb_h};
	return flux_rec_fill_rounded(b, &thumb, 1.5f, t->ctrl_strong_fill_default);
}

static bool combo_record_chrome(XentDlBuilder *b, FluxComboRuntime const *rt, FluxColor bg, FluxColor border) {
	FluxRect bounds = {0.0f, 0.0f, rt->width, rt->height};
	if (!flux_rec_fill_rounded(b, &bounds, CB_CORNER, bg)) return false;
	float    half  = CB_BORDER * 0.5f;
	FluxRect inset = {bounds.x + half, bounds.y + half, bounds.w - half * 2.0f, bounds.h - half * 2.0f};
	return flux_rec_stroke_rounded(b, &inset, CB_CORNER, border, CB_BORDER);
}

static bool combo_record_rows(XentDlBuilder *b, FluxComboRuntime const *rt, FluxThemeColors const *theme, float vp) {
	FluxRect view = {CB_BORDER, CB_BORDER + CB_CONTENT_MARGIN_Y, rt->width - 2.0f * CB_BORDER, vp};
	if (!xent_dl_pushclip_rect(b, flux_rec_xrect(&view))) return false;
	int first = rt->row_h > 0.0f ? ( int ) (rt->scroll_y / rt->row_h) : 0;
	int last  = rt->row_h > 0.0f ? ( int ) ((rt->scroll_y + vp) / rt->row_h) + 1 : rt->model.item_count - 1;
	if (first < 0) first = 0;
	if (last > rt->model.item_count - 1) last = rt->model.item_count - 1;
	bool ok = true;
	for (int i = first; ok && i <= last; i++) ok = combo_rec_item(b, rt, theme, i);
	return xent_dl_popclip(b) && ok;
}

static XentDl *
combo_record_popup(FluxComboRuntime const *rt, FluxThemeColors const *theme, FluxColor bg, FluxColor border) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	float vp = combo_viewport_h(rt);
	bool  ok = combo_record_chrome(b, rt, bg, border);
	if (ok) ok = combo_record_rows(b, rt, theme, vp);
	if (ok) ok = combo_rec_scrollbar(b, rt, theme, vp);
	return flux_rec_finish(b, ok);
}

static XentDl *combo_record(void *ctx, FluxPopup const *popup) {
	FluxComboRuntime      *rt      = ( FluxComboRuntime * ) ctx;
	FluxThemeColors const *theme   = NULL;
	bool                   is_dark = false;
	combo_resolve_theme(rt, &theme, &is_dark);

	FluxColor bg     = is_dark ? flux_color_rgb(0x2c, 0x2c, 0x2c) : flux_color_rgb(0xf9, 0xf9, 0xf9);
	FluxColor border = is_dark ? flux_color_rgba(0, 0, 0, 0x33) : flux_color_rgba(0, 0, 0, 0x0f);
	bg               = flux_popup_acrylic_tint(popup, bg);
	return combo_record_popup(rt, theme, bg, border);
}

static void combo_repaint_popup(FluxComboRuntime *rt) {
	HWND h = flux_popup_hwnd(rt->popup);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static void combo_repaint_owner(FluxComboRuntime *rt) {
	HWND h = flux_window_hwnd(rt->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static void combo_update_layout_text(FluxComboRuntime *rt) {
	char const *text = NULL;
	if (rt->model.selected_index >= 0 && rt->model.selected_index < rt->model.item_count)
		text = rt->model.items [rt->model.selected_index];
	if (!text || !text [0]) text = rt->model.placeholder;
	xent_settext(rt->ctx, rt->node, text);
}

static void combo_close(FluxComboRuntime *rt) {
	rt->model.open = false;
	flux_popup_dismiss(rt->popup);
	combo_repaint_owner(rt);
}

static void combo_notify(FluxComboRuntime *rt, int index) {
	void  (*notify)(void *, int) = rt->model.on_select;
	void *notify_ctx             = rt->model.on_select_ctx;
	flux_input_route_event(
	  flux_store_host(rt->store), rt->node, XTK_EVENT_SELECT,
	  (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index}, XTK_EVENT_CAUSE_USER
	);
	if (notify) notify(notify_ctx, index);
}

static void combo_commit(FluxComboRuntime *rt, int index) {
	if (index < 0 || index >= rt->model.item_count) return;
	rt->model.selected_index = index;
	combo_update_layout_text(rt);
	combo_close(rt);
	combo_notify(rt, index);
}

/* Drop-down width: at least the box width, grown to the widest item. */
static float combo_dropdown_width(FluxComboRuntime const *rt, float base_w) {
	if (!rt->text) return base_w;
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_FONT_SIZE_DEFAULT;
	ts.font_weight = FLUX_FONT_REGULAR;
	float pad      = 2.0f * (CB_BORDER + CB_ITEM_MARGIN_X) + CB_ITEM_PAD_LEFT + 4.0f;
	for (int i = 0; i < rt->model.item_count; i++) {
		if (!rt->model.items [i]) continue;
		float w = flux_text_measure(rt->text, rt->model.items [i], &ts, 0).w + pad;
		if (w > base_w) base_w = w;
	}
	return base_w;
}

static void combo_open(FluxComboRuntime *rt) {
	XentRect r = {0};
	xent_layout_rect(rt->ctx, rt->node, &r);

	/* WinUI grows drop-down items for touch (ComboBoxItemThemeTouchPadding). The
	 * device that opened the box decides the item height for this session. */
	FluxNodeData *nd    = flux_store_find(rt->store, rt->node);
	bool          touch = nd && nd->state.pointer_type == 1;
	rt->row_h           = touch ? CB_ROW_H_TOUCH : CB_ROW_H;

	rt->width           = combo_dropdown_width(rt, r.w > 0.0f ? r.w : 120.0f);
	rt->height          = combo_dropdown_height(rt->model.item_count, rt->row_h);
	rt->content_h       = ( float ) rt->model.item_count * rt->row_h;
	flux_popup_set_size(rt->popup, rt->width, rt->height);

	rt->model.open = true;
	rt->highlight  = rt->model.selected_index;
	rt->scroll_y   = 0.0f;
	combo_ensure_visible(rt, rt->highlight);
	FluxRect anchor = flux_binding_screen_anchor(rt->window, rt->ctx, rt->store, rt->node);
	flux_popup_show(rt->popup, anchor, FLUX_PLACEMENT_BOTTOM);
	combo_repaint_owner(rt);
}

/* WinUI ComboBox opens its drop-down on pointer release (OnPointerReleased), not press --
 * press only sets the pressed visual. Firing on click also gives press-cancel for free. */
static void combo_on_click(void *ctx) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt) return;
	if (rt->model.open) combo_close(rt);
	else combo_open(rt);
}

/* A held touch contact that travels past the slop becomes a pan: the pressed
 * item is released (no selection on lift) and the list scrolls with the
 * finger. Mouse drags never pan — they cancel via the release-on-same-item
 * rule, and mouse scrolling stays on the wheel. */
static bool combo_touch_pan(FluxComboRuntime *rt, FluxPopup *popup, float y) {
	if (rt->pressed_index < 0 && !rt->panning) return false;
	if (!flux_popup_lasttouch(popup)) return false;
	if (!rt->panning) {
		if (fabsf(y - rt->press_y) < CB_TOUCH_PAN_SLOP) return false;
		rt->panning       = true;
		rt->pressed_index = -1;
		rt->highlight     = -1;
	}
	rt->scroll_y = rt->pan_start_scroll + (rt->press_y - y);
	combo_clamp_scroll(rt);
	combo_repaint_popup(rt);
	return true;
}

static void combo_mouse_down(FluxComboRuntime *rt, float y) {
	rt->pressed_index    = combo_item_at(rt, y);
	rt->panning          = false;
	rt->press_y          = y;
	rt->pan_start_scroll = rt->scroll_y;
	combo_repaint_popup(rt);
}

static void combo_mouse_up(FluxComboRuntime *rt, float y) {
	int  idx          = combo_item_at(rt, y);
	bool commit       = !rt->panning && idx >= 0 && idx == rt->pressed_index;
	rt->pressed_index = -1;
	rt->panning       = false;
	combo_repaint_popup(rt);
	if (commit) combo_commit(rt, idx);
}

static void combo_mouse_wheel(FluxComboRuntime *rt, float dy) {
	rt->scroll_y -= dy;
	combo_clamp_scroll(rt);
	combo_repaint_popup(rt);
}

static void combo_mouse_hover(FluxComboRuntime *rt, FluxPopupMouseEvent event, float y) {
	int hot = (event == FLUX_POPUP_MOUSE_LEAVE) ? -1 : combo_item_at(rt, y);
	if (hot == rt->highlight) return;
	rt->highlight = hot;
	combo_repaint_popup(rt);
}

static void combo_mouse(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y) {
	( void ) x;
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt) return;
	if (event == FLUX_POPUP_MOUSE_DOWN) {
		combo_mouse_down(rt, y);
		return;
	}
	if (event == FLUX_POPUP_MOUSE_UP) {
		combo_mouse_up(rt, y);
		return;
	}
	if (event == FLUX_POPUP_MOUSE_WHEEL) {
		combo_mouse_wheel(rt, y);
		return;
	}
	if (event == FLUX_POPUP_MOUSE_MOVE && combo_touch_pan(rt, popup, y)) return;
	combo_mouse_hover(rt, event, y);
}

static void combo_step_selection(FluxComboRuntime *rt, int delta) {
	int n = rt->model.item_count;
	if (n <= 0) return;
	int next = rt->model.selected_index + delta;
	if (next < 0) next = 0;
	if (next >= n) next = n - 1;
	if (next == rt->model.selected_index) return;
	rt->model.selected_index = next;
	combo_update_layout_text(rt);
	combo_repaint_owner(rt);
	combo_notify(rt, next);
}

static void combo_step_highlight(FluxComboRuntime *rt, int delta) {
	int n = rt->model.item_count;
	if (n <= 0) return;
	int next = (rt->highlight < 0 ? rt->model.selected_index : rt->highlight) + delta;
	if (next < 0) next = 0;
	if (next >= n) next = n - 1;
	rt->highlight = next;
	combo_ensure_visible(rt, next);
	combo_repaint_popup(rt);
}

static void combo_jump_selection(FluxComboRuntime *rt, int index) {
	int n = rt->model.item_count;
	if (n <= 0) return;
	if (index < 0) index = 0;
	if (index >= n) index = n - 1;
	if (index == rt->model.selected_index) return;
	rt->model.selected_index = index;
	combo_update_layout_text(rt);
	combo_repaint_owner(rt);
	combo_notify(rt, index);
}

static void combo_jump_highlight(FluxComboRuntime *rt, int index) {
	int n = rt->model.item_count;
	if (n <= 0) return;
	if (index < 0) index = 0;
	if (index >= n) index = n - 1;
	rt->highlight = index;
	combo_ensure_visible(rt, index);
	combo_repaint_popup(rt);
}

static bool combo_alt_down(void) { return flux_key_held(FLUX_KEY_ALT); }

static int  combo_page_rows(FluxComboRuntime *rt) {
	float vp = combo_viewport_h(rt);
	int   n  = rt->row_h > 0.0f ? ( int ) (vp / rt->row_h) : 1;
	return n > 0 ? n : 1;
}

static bool combo_toggle_popup_key(FluxComboRuntime *rt, FluxKey key, bool closing) {
	if (key != FLUX_KEY_F4 && !((key == FLUX_KEY_DOWN || key == FLUX_KEY_UP) && combo_alt_down())) return false;
	if (closing) combo_close(rt);
	else combo_open(rt);
	return true;
}

static bool combo_key_nav_collapsed(FluxComboRuntime *rt, FluxKey key) {
	if (key == FLUX_KEY_DOWN) {
		combo_step_selection(rt, +1);
		return true;
	}
	if (key == FLUX_KEY_UP) {
		combo_step_selection(rt, -1);
		return true;
	}
	if (key == FLUX_KEY_HOME) {
		combo_jump_selection(rt, 0);
		return true;
	}
	if (key == FLUX_KEY_END) {
		combo_jump_selection(rt, rt->model.item_count - 1);
		return true;
	}
	return false;
}

static bool combo_key_collapsed(FluxComboRuntime *rt, FluxKey key) {
	if (combo_toggle_popup_key(rt, key, false)) return true;
	if (combo_key_nav_collapsed(rt, key)) return true;
	if (key == FLUX_KEY_ENTER || key == FLUX_KEY_SPACE) {
		combo_open(rt);
		return true;
	}
	return false;
}

static bool combo_key_nav_open(FluxComboRuntime *rt, FluxKey key) {
	if (key == FLUX_KEY_DOWN) {
		combo_step_highlight(rt, +1);
		return true;
	}
	if (key == FLUX_KEY_UP) {
		combo_step_highlight(rt, -1);
		return true;
	}
	if (key == FLUX_KEY_HOME) {
		combo_jump_highlight(rt, 0);
		return true;
	}
	if (key == FLUX_KEY_END) {
		combo_jump_highlight(rt, rt->model.item_count - 1);
		return true;
	}
	if (key == FLUX_KEY_PAGE_UP) {
		combo_step_highlight(rt, -combo_page_rows(rt));
		return true;
	}
	if (key == FLUX_KEY_PAGE_DOWN) {
		combo_step_highlight(rt, +combo_page_rows(rt));
		return true;
	}
	return false;
}

static bool combo_key_action_open(FluxComboRuntime *rt, FluxKey key) {
	if (key == FLUX_KEY_LEFT || key == FLUX_KEY_RIGHT) return true;
	if (key == FLUX_KEY_SPACE || key == FLUX_KEY_ENTER) {
		combo_commit(rt, rt->highlight);
		return true;
	}
	if (key == FLUX_KEY_ESCAPE) {
		combo_close(rt);
		return true;
	}
	if (key == FLUX_KEY_TAB) {
		combo_close(rt);
		return false;
	}
	return false;
}

static bool combo_key_open(FluxComboRuntime *rt, FluxKey key) {
	if (combo_toggle_popup_key(rt, key, true)) return true;
	if (combo_key_nav_open(rt, key)) return true;
	return combo_key_action_open(rt, key);
}

static bool combo_on_key(void *ctx, FluxKey key, bool down) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt || !down) return false;
	return rt->model.open ? combo_key_open(rt, key) : combo_key_collapsed(rt, key);
}

/* Case-insensitive ASCII prefix match against the typeahead buffer. */
static bool combo_prefix_match(FluxComboRuntime const *rt, char const *s) {
	if (!s) return false;
	for (int k = 0; k < rt->search_len; k++)
		if (!s [k] || ( wchar_t ) towlower(( unsigned char ) s [k]) != rt->search [k]) return false;
	return true;
}

static void combo_search_push(FluxComboRuntime *rt, wchar_t ch) {
	DWORD now = GetTickCount();
	if (now - rt->search_last > 1000 || rt->search_len >= ( int ) (sizeof(rt->search) / sizeof(rt->search [0])) - 1)
		rt->search_len = 0;
	rt->search [rt->search_len++] = ( wchar_t ) towlower(ch);
	rt->search [rt->search_len]   = 0;
	rt->search_last               = now;
}

static int combo_search_hit(FluxComboRuntime const *rt) {
	int n     = rt->model.item_count;
	int start = rt->model.open ? rt->highlight : rt->model.selected_index;
	for (int step = rt->search_len > 1 ? 0 : 1; step <= n; step++) {
		int i = (((start < 0 ? -1 : start) + step) % n + n) % n;
		if (combo_prefix_match(rt, rt->model.items [i])) return i;
	}
	return -1;
}

static void combo_on_char(void *ctx, wchar_t ch) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt || rt->model.item_count <= 0 || ch < 32) return;
	combo_search_push(rt, ch);
	int hit = combo_search_hit(rt);
	if (hit < 0) return;
	if (rt->model.open) combo_jump_highlight(rt, hit);
	else combo_jump_selection(rt, hit);
}

static void combo_destroy(void *component_data) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) component_data;
	if (!rt) return;
	for (int i = 0; i < rt->model.item_count; i++) flux_str_free(rt->model.items [i]);
	free(( void * ) rt->model.items);
	flux_str_free(rt->model.placeholder);
	if (rt->popup) flux_popup_destroy(rt->popup);
	free(rt);
}

/* Deep-copy the caller's item array so the model owns its strings. */
static char const **combo_copy_items(char const *const *items, int count) {
	if (!items || count <= 0) return NULL;
	char const **copy = ( char const ** ) calloc(( size_t ) count, sizeof(*copy));
	if (!copy) return NULL;
	for (int i = 0; i < count; i++) copy [i] = flux_str_dup(items [i]);
	return copy;
}

static FluxComboRuntime *combo_runtime(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_COMBO_BOX || !nd->component_data) return NULL;
	return ( FluxComboRuntime * ) nd->component_data;
}

void flux_prop_combo_selected(FluxStore *store, XentNodeId id, int index) {
	FluxComboRuntime *rt = combo_runtime(store, id);
	if (!rt) return;
	if (index < -1 || index >= rt->model.item_count) index = -1;
	rt->model.selected_index = index;
	combo_update_layout_text(rt);
}

void flux_prop_combo_placeholder(FluxStore *store, XentNodeId id, char const *placeholder) {
	FluxComboRuntime *rt = combo_runtime(store, id);
	if (!rt) return;
	char *copy = flux_str_dup(placeholder);
	if (placeholder && !copy) return;
	flux_str_free(rt->model.placeholder);
	rt->model.placeholder = copy;
	combo_update_layout_text(rt);
}

/* Reserve the widest item's width as the closed box's minimum so its width stays
 * constant across selection (WinUI-style) instead of shrinking to the selected text. */
static void combo_apply_min_width(FluxComboRuntime *rt) {
	if (!rt->text || rt->node == XENT_NODE_INVALID) return;
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_FONT_SIZE_DEFAULT;
	ts.font_weight = FLUX_FONT_REGULAR;
	float pad      = 12.0f + 32.0f; /* closed-box padding: 12 text inset + 32 chevron column */
	float w        = 80.0f;         /* keep the existing floor */
	for (int i = 0; i < rt->model.item_count; i++) {
		if (!rt->model.items [i]) continue;
		float iw = flux_text_measure(rt->text, rt->model.items [i], &ts, 0).w + pad;
		if (iw > w) w = iw;
	}
	xent_setminsize(rt->ctx, rt->node, (XentSize) {w, 32.0f});
}

void flux_prop_combo_items(FluxStore *store, XentNodeId id, char const *const *items, int count) {
	FluxComboRuntime *rt = combo_runtime(store, id);
	if (!rt) return;

	char const **copy = combo_copy_items(items, count);
	for (int i = 0; i < rt->model.item_count; i++) flux_str_free(rt->model.items [i]);
	free(( void * ) rt->model.items);

	rt->model.items      = copy;
	rt->model.item_count = copy ? count : 0;
	if (rt->model.selected_index >= rt->model.item_count) rt->model.selected_index = -1;
	if (rt->highlight >= rt->model.item_count) rt->highlight = -1;
	rt->content_h = ( float ) rt->model.item_count * rt->row_h;
	combo_clamp_scroll(rt);
	combo_update_layout_text(rt);
	combo_apply_min_width(rt);
}

static void combo_dismissed(void *ctx) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt) return;
	rt->model.open = false;
	combo_repaint_owner(rt);
}

XentNodeId flux_combo_init(FluxComboBoxDesc const *info) {
	if (!info || !info->ctx || !info->store || !info->window) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_COMBO_BOX);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_store_find(info->store, node);
	FluxComboRuntime *rt = nd ? ( FluxComboRuntime * ) calloc(1, sizeof(*rt)) : NULL;
	if (!nd || !rt) {
		free(rt);
		return node;
	}
	rt->model.items          = combo_copy_items(info->items, info->item_count);
	rt->model.item_count     = rt->model.items ? info->item_count : 0;
	rt->model.selected_index = info->selected_index;
	rt->model.placeholder    = flux_str_dup(info->placeholder);
	rt->model.on_select      = info->on_select;
	rt->model.on_select_ctx  = info->userdata;
	rt->store                = info->store;
	rt->node                 = node;
	rt->ctx                  = info->ctx;
	rt->window               = info->window;
	rt->text                 = info->text;
	rt->theme                = info->theme;
	rt->highlight            = -1;
	rt->pressed_index        = -1;

	rt->popup                = flux_popup_create(info->window);
	if (rt->popup) {
		flux_popup_setdismissoutside(rt->popup, true);
		flux_popup_setanim(rt->popup, FLUX_POPUP_ANIM_FLYOUT);
		flux_popup_onrecord(rt->popup, combo_record, rt, rt->text);
		flux_popup_onmouse(rt->popup, combo_mouse, rt);
		flux_popup_ondismiss(rt->popup, combo_dismissed, rt);
	}

	nd->component_data         = &rt->model;
	nd->destroy_component_data = combo_destroy;
	nd->behavior.on_click      = combo_on_click;
	nd->behavior.on_click_ctx  = rt;
	nd->behavior.on_key        = combo_on_key;
	nd->behavior.on_key_ctx    = rt;
	nd->behavior.on_char       = combo_on_char;
	nd->behavior.on_char_ctx   = rt;

	xent_set_focusable(info->ctx, node, true);
	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_BUTTON);
	xent_setsize(info->ctx, node, (XentSize) {NAN, 32.0f});
	xent_setminsize(info->ctx, node, (XentSize) {80.0f, 32.0f});
	xent_setp(info->ctx, node, (XentInsets) {12.0f, 0, 32.0f, 0});
	combo_update_layout_text(rt);
	combo_apply_min_width(rt);
	return node;
}
