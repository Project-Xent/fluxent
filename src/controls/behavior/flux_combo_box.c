#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "bridge/flux_internal.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_fluent.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "controls/data/flux_textbox_data.h"
#include "controls/factory/flux_init_popup.h"
#include "controls/factory/flux_init_textbox.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_window.h"

#include <math.h>

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define CB_ROW_H            36.0f
#define CB_ROW_H_TOUCH      48.0f
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
	int              hover_index;
	bool             kb_focus;
	DWORD            press_tick;
	float            bar_hover_x;
	float            bar_hover_y;
	float            bar_expand;
	DWORD            bar_tick;
	bool             bar_drag;
	float            bar_drag_y;
	float            bar_drag_scroll;
	int              selection_at_open;
	int              pressed_index;
	float            width;
	float            height;
	float            scroll_y;
	float            content_h;
	float            row_h;
	bool             panning;
	float            press_y;
	float            pan_start_scroll;

	wchar_t          search [64];
	int              search_len;
	DWORD            search_last;

	FluxApp         *app;
	FluxInput       *input;
	XentNodeId       editor;
	char const      *content;
	bool             ignore_text;
	bool             (*editor_on_key)(void *ctx, FluxKey key, bool down);
	void            *editor_on_key_ctx;
	void             (*editor_on_down)(void *ctx, float x, float y, int clicks);
	void            *editor_on_down_ctx;
	void             (*editor_on_click)(void *ctx);
	void            *editor_on_click_ctx;
	void             (*editor_on_blur)(void *ctx);
	void            *editor_on_blur_ctx;
	void             (*editor_on_focus)(void *ctx);
	void            *editor_on_focus_ctx;
	void             (*editor_on_char)(void *ctx, wchar_t ch);
	void            *editor_on_char_ctx;
	char const      *edit_search;
	char const      *custom_value;
	char const      *restore_custom;
	int              restore_index;
	int              search_result;
	bool             restore_set;
	bool             search_set;
	bool             overlay_down;
} FluxComboRuntime;

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

static FluxColor combo_item_fill(FluxThemeColors const *t, bool selected, bool hot, bool pressed) {
	if (selected) return pressed ? t->subtle_fill_secondary : hot ? t->subtle_fill_tertiary : t->subtle_fill_secondary;
	if (pressed) return t->subtle_fill_tertiary;
	if (hot) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

static bool combo_rec_item_label(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *item, char const *label, bool pressed
) {
	if (!label || !label [0]) return true;
	FluxRect tr = {item->x + CB_ITEM_PAD_LEFT, item->y + 5.0f, item->w - CB_ITEM_PAD_LEFT - 4.0f, item->h - 12.0f};
	if (tr.w <= 0.0f) return true;
	XentTextStyle style
	  = {.font_size = FLUX_FONT_SIZE_DEFAULT, .font_weight = FLUX_FONT_REGULAR, .h_align = 0u, .v_align = 1u};
	return flux_rec_text_ex(b, &tr, label, &style, pressed ? t->text_secondary : t->text_primary);
}

static void combo_repaint_popup(FluxComboRuntime *rt);

#define CB_PILL_SQUASH_MS 167.0f

static float combo_pill_scale(FluxComboRuntime const *rt, bool pressed) {
	if (!pressed) return 1.0f;
	float t = ( float ) (flux_anim_now() - rt->press_tick) / CB_PILL_SQUASH_MS;
	return 1.0f - 0.375f * flux_cubic_bezier(t > 1.0f ? 1.0f : t, 0.0f, 0.0f, 0.0f, 1.0f);
}

static bool combo_pill_step(void *ctx, unsigned long now) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	combo_repaint_popup(rt);
	return rt->pressed_index >= 0 && ( float ) (( DWORD ) now - rt->press_tick) < CB_PILL_SQUASH_MS;
}

static bool combo_rec_item(XentDlBuilder *b, FluxComboRuntime const *rt, FluxThemeColors const *t, int i) {
	float    row  = CB_BORDER + CB_CONTENT_MARGIN_Y + ( float ) i * rt->row_h - rt->scroll_y;
	FluxRect item = {
	  CB_BORDER + CB_ITEM_MARGIN_X, row + CB_ITEM_MARGIN_Y, rt->width - 2.0f * (CB_BORDER + CB_ITEM_MARGIN_X),
	  rt->row_h - 2.0f * CB_ITEM_MARGIN_Y};

	bool      selected = i == rt->model.selected_index;
	bool      pressed  = i == rt->pressed_index;
	FluxColor fill     = combo_item_fill(t, selected, i == rt->hover_index, pressed);
	bool      ok       = true;
	if (flux_color_af(fill) > 0.0f) ok = flux_rec_fill_rounded(b, &item, CB_ITEM_CORNER, fill);
	if (ok && selected) {
		float    h    = CB_PILL_H * combo_pill_scale(rt, pressed);
		FluxRect pill = {item.x, item.y + (item.h - h) * 0.5f, CB_PILL_W, h};
		ok            = flux_rec_fill_rounded(b, &pill, CB_PILL_CORNER, t->accent_default);
	}
	ok = ok && combo_rec_item_label(b, t, &item, rt->model.items [i], pressed);
	if (ok && rt->kb_focus && i == rt->highlight) ok = flux_rec_focus_rect(b, t, &item, CB_ITEM_CORNER);
	return ok;
}

static FluxRect combo_bar_rect(FluxComboRuntime const *rt) {
	return (FluxRect) {
	  rt->width - CB_BORDER - FLUX_SCROLLBAR_SIZE, CB_BORDER, FLUX_SCROLLBAR_SIZE, rt->height - 2.0f * CB_BORDER};
}

static FluxScrollBarModel combo_bar_model(FluxComboRuntime const *rt) {
	return (FluxScrollBarModel) {0.0f, combo_max_scroll(rt), rt->scroll_y, combo_viewport_h(rt), true};
}

static bool combo_rec_scrollbar(XentDlBuilder *b, FluxComboRuntime const *rt, FluxThemeColors const *t, float vp) {
	( void ) vp;
	if (combo_max_scroll(rt) <= 0.0f) return true;
	FluxRect           bar   = combo_bar_rect(rt);
	FluxScrollBarModel model = combo_bar_model(rt);
	return flux_rec_scrollbar_mouse(b, t, &bar, &model, rt->bar_hover_x, rt->bar_hover_y, rt->bar_expand);
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
	if (!xent_dl_pushcliprect(b, flux_rec_xrect(&view))) return false;
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
	bg               = flux_popup_acrylictint(popup, bg);
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

static char const *combo_selected_text(FluxComboRuntime const *rt) {
	if (rt->model.selected_index < 0 || rt->model.selected_index >= rt->model.item_count) return NULL;
	return rt->model.items [rt->model.selected_index];
}

static void combo_editor_write(FluxComboRuntime *rt, char const *text);

static void combo_update_layout_text(FluxComboRuntime *rt) {
	char const *text = combo_selected_text(rt);
	if (rt->model.editable) {
		xent_settext(rt->ctx, rt->node, "");
		if (text) combo_editor_write(rt, text);
		return;
	}
	if (!text || !text [0]) text = rt->model.placeholder;
	xent_settext(rt->ctx, rt->node, text);
}

static void combo_set_selected(FluxComboRuntime *rt, int index) {
	rt->model.selected_index = index;
	if (!rt->model.editable) return;
	rt->search_set    = true;
	rt->search_result = index;
	flux_str_replace(&rt->edit_search, combo_selected_text(rt));
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
	bool changed = rt->model.selected_index != index;
	combo_set_selected(rt, index);
	combo_update_layout_text(rt);
	combo_close(rt);
	if (changed) combo_notify(rt, index);
}

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

static int  combo_open_highlight(FluxComboRuntime const *rt);
static void combo_apply_min_width(FluxComboRuntime *rt);

typedef struct ComboLayout {
	double above, below, h, max_h, win_h;
	int    ia, ib, n, laid, max_side, max_all;
} ComboLayout;

static void combo_layout_fill_above(ComboLayout *l, double upper) {
	while (l->ia >= 0 && l->above - l->h >= upper && l->laid < l->max_side) {
		l->above -= l->h;
		l->laid++;
		l->ia--;
	}
}

static void combo_layout_fill_below(ComboLayout *l, double lower) {
	while (l->ib < l->n && l->below + l->h < lower && l->below - l->above < l->max_h && l->laid < l->max_all) {
		l->below += l->h;
		l->laid++;
		l->ib++;
	}
}

static bool combo_layout_room(ComboLayout const *l) {
	bool fits = l->below - l->above + l->h <= l->max_h;
	return fits && (l->below + l->h < l->win_h || l->above - l->h >= 0.0) && l->laid < l->max_all;
}

static void combo_layout_fill_rest(ComboLayout *l) {
	while ((l->ia >= 0 || l->ib < l->n) && combo_layout_room(l)) {
		if (l->ia >= 0) l->ia--;
		else l->ib++;
		if (l->above - l->h <= 0.0) l->below += l->h;
		else l->above -= l->h;
		l->laid++;
	}
}

static ComboLayout combo_layout_centered(int center, int n, double h, double cb_y, double cb_h, double win_h) {
	double      margin = CB_CONTENT_MARGIN_Y;
	ComboLayout l      = {.h = h, .max_h = CB_MAX_HEIGHT, .win_h = win_h, .n = n, .laid = 1};
	l.max_side         = n < 7 ? n : 7;
	l.max_all          = n < 15 ? n : 15;
	if (center < 0 || center > n) center = n / 2;
	if (cb_y + cb_h >= win_h) cb_y = win_h - cb_h;
	double calc_above = cb_y + cb_h / 2.0 - h / 2.0 - margin;
	l.above           = calc_above > 0.0 ? calc_above : 0.0;
	double upper      = fmax(cb_y + cb_h / 2.0 - l.max_h / 2.0, 0.0);
	double calc_below = l.above + h + 2.0 * margin;
	l.below           = fmin(calc_below, win_h);
	double lower      = fmin(upper + l.max_h, win_h);
	if (calc_below > win_h) l.above = fmax(l.above - calc_below + win_h, 0.0);
	l.ia = center - 1;
	l.ib = center + 1;
	combo_layout_fill_above(&l, upper);
	combo_layout_fill_below(&l, lower);
	combo_layout_fill_rest(&l);
	return l;
}

static void combo_show_centered(FluxComboRuntime *rt, FluxRect anchor) {
	HMONITOR    mon = MonitorFromPoint((POINT) {( LONG ) anchor.x, ( LONG ) anchor.y}, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi  = {.cbSize = sizeof(mi)};
	UINT        dpi = GetDpiForWindow(flux_window_hwnd(rt->window));
	double      s   = dpi > 0 ? dpi / ( double ) FLUX_DPI_BASE : 1.0;
	if (!GetMonitorInfoW(mon, &mi))
		mi.rcWork = (RECT) {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
	double      win_h = (mi.rcWork.bottom - mi.rcWork.top) / s;
	double      cb_y  = (anchor.y - mi.rcWork.top) / s;
	ComboLayout l
	  = combo_layout_centered(rt->model.selected_index, rt->model.item_count, rt->row_h, cb_y, anchor.h / s, win_h);
	double h = l.below - l.above + 2.0 * CB_BORDER;
	double y = l.above - CB_BORDER;
	if (y + h > win_h) y = fmax(y - (y + h - win_h), 0.0);
	rt->height   = ( float ) h;
	rt->scroll_y = ( float ) (l.ia + 1) * rt->row_h;
	flux_popup_setsize(rt->popup, rt->width, rt->height);
	double box_mid = cb_y + anchor.h / s * 0.5;
	flux_popup_setsplit(rt->popup, ( float ) (anchor.h / s), ( float ) (box_mid - (y + h * 0.5)));
	FluxRect at = {anchor.x, ( float ) (mi.rcWork.top + y * s), 0.0f, 0.0f};
	flux_popup_show(rt->popup, at, FLUX_PLACEMENT_BOTTOM);
}

static void combo_open(FluxComboRuntime *rt) {
	XentRect r = {0};
	xent_layout_rect(rt->ctx, rt->node, &r);

	FluxNodeData *nd    = flux_store_find(rt->store, rt->node);
	bool          touch = nd && nd->state.pointer_type == 1;
	rt->row_h           = touch ? CB_ROW_H_TOUCH : CB_ROW_H;

	rt->width           = flux_maxf(combo_dropdown_width(rt, r.w > 0.0f ? r.w : 120.0f), touch ? 240.0f : 80.0f);
	rt->height          = combo_dropdown_height(rt->model.item_count, rt->row_h);
	rt->content_h       = ( float ) rt->model.item_count * rt->row_h;
	flux_popup_setsize(rt->popup, rt->width, rt->height);

	rt->selection_at_open = rt->model.selected_index;
	bool select_first     = !rt->model.editable && rt->selection_at_open < 0 && rt->model.item_count > 0;
	if (select_first) {
		rt->model.selected_index = 0;
		combo_update_layout_text(rt);
	}
	rt->model.open  = true;
	rt->kb_focus    = false;
	rt->hover_index = -1;
	rt->highlight   = combo_open_highlight(rt);
	rt->scroll_y    = 0.0f;
	FluxRect anchor = flux_binding_screen_anchor(rt->window, rt->ctx, rt->store, rt->node);
	if (!rt->model.editable && rt->model.item_count > 0 && rt->window) combo_show_centered(rt, anchor);
	else {
		combo_ensure_visible(rt, rt->highlight);
		flux_popup_show(rt->popup, anchor, FLUX_PLACEMENT_BOTTOM);
	}
	combo_repaint_owner(rt);
	if (select_first) combo_notify(rt, 0);
}

static bool combo_hit_overlay(FluxComboRuntime const *rt) {
	FluxNodeData *nd   = flux_store_find(rt->store, rt->node);
	XentRect      rect = {0};
	xent_layout_rect(rt->ctx, rt->node, &rect);
	return nd && nd->hover_local_x >= rect.w - FLUX_COMBO_GLYPH_COL;
}

static void combo_editor_expand(FluxComboRuntime *rt, bool on);
static void combo_editor_selectall(FluxComboRuntime *rt);

static void combo_toggle(FluxComboRuntime *rt) {
	if (rt->model.open) combo_close(rt);
	else combo_open(rt);
}

static void combo_on_click(void *ctx) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt) return;
	if (!rt->model.editable || combo_hit_overlay(rt)) combo_toggle(rt);
}

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
	rt->press_tick       = flux_anim_now();
	rt->kb_focus         = false;
	if (rt->pressed_index >= 0 && rt->pressed_index == rt->model.selected_index)
		flux_anim_register(rt, combo_pill_step);
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

static void combo_repaint_popup(FluxComboRuntime *rt);

static bool combo_bar_step(void *ctx, unsigned long now) {
	FluxComboRuntime *rt      = ( FluxComboRuntime * ) ctx;
	float             target  = (rt->bar_hover_x >= 0.0f || rt->bar_drag) ? 1.0f : 0.0f;
	float             dt      = ( float ) (( DWORD ) now - rt->bar_tick) / 1000.0f;
	rt->bar_tick              = ( DWORD ) now;
	rt->bar_expand           += (target - rt->bar_expand) * (1.0f - expf(-dt * 16.0f));
	if (fabsf(target - rt->bar_expand) < 0.001f) rt->bar_expand = target;
	combo_repaint_popup(rt);
	return rt->bar_expand != target;
}

static bool combo_bar_track(FluxComboRuntime *rt, float x, float y) {
	FluxRect bar    = combo_bar_rect(rt);
	bool     on     = combo_max_scroll(rt) > 0.0f && flux_rect_contains(&bar, x, y);
	float    prev   = rt->bar_hover_x;
	rt->bar_hover_x = on ? x - bar.x : -1.0f;
	rt->bar_hover_y = on ? y - bar.y : -1.0f;
	if ((prev >= 0.0f) != on) {
		rt->bar_tick = flux_anim_now();
		flux_anim_register(rt, combo_bar_step);
	}
	return on;
}

static void combo_bar_press(FluxComboRuntime *rt, float x, float y) {
	FluxRect           bar   = combo_bar_rect(rt);
	FluxScrollBarModel model = combo_bar_model(rt);
	FluxScrollBarParts parts = {0};
	flux_scroll_bar_parts(&parts, &bar, &model, 1.0f);
	if (flux_rect_contains(&parts.thumb, x, y)) {
		rt->bar_drag        = true;
		rt->bar_drag_y      = y;
		rt->bar_drag_scroll = rt->scroll_y;
		return;
	}
	float page = combo_viewport_h(rt);

	struct {
		FluxRect const *r;
		float           delta;
	} const zones [] = {
	  {&parts.dec_btn,   -16.0f},
      {&parts.inc_btn,   16.0f },
      {&parts.large_dec, -page },
      {&parts.large_inc, page  }
    };

	for (size_t i = 0; i < sizeof(zones) / sizeof(zones [0]); ++i)
		if (flux_rect_contains(zones [i].r, x, y)) rt->scroll_y += zones [i].delta;
	combo_clamp_scroll(rt);
	combo_repaint_popup(rt);
}

static void combo_bar_drag(FluxComboRuntime *rt, float y) {
	FluxRect           bar   = combo_bar_rect(rt);
	FluxScrollBarModel model = combo_bar_model(rt);
	FluxScrollBarParts parts = {0};
	flux_scroll_bar_parts(&parts, &bar, &model, 1.0f);
	float travel = parts.track.h - parts.thumb.h;
	if (travel <= 0.0f) return;
	rt->scroll_y = rt->bar_drag_scroll + (y - rt->bar_drag_y) * combo_max_scroll(rt) / travel;
	combo_clamp_scroll(rt);
	combo_repaint_popup(rt);
}

static void combo_mouse_hover(FluxComboRuntime *rt, FluxPopupMouseEvent event, float y) {
	int hot = (event == FLUX_POPUP_MOUSE_LEAVE) ? -1 : combo_item_at(rt, y);
	if (hot == rt->hover_index) return;
	rt->hover_index = hot;
	combo_repaint_popup(rt);
}

static bool combo_bar_mouse(FluxComboRuntime *rt, FluxPopupMouseEvent event, float x, float y) {
	if (event == FLUX_POPUP_MOUSE_WHEEL) return false;
	if (event == FLUX_POPUP_MOUSE_UP && rt->bar_drag) {
		rt->bar_drag = false;
		flux_anim_register(rt, combo_bar_step);
		return true;
	}
	if (event == FLUX_POPUP_MOUSE_MOVE && rt->bar_drag) {
		combo_bar_drag(rt, y);
		return true;
	}
	bool on = combo_bar_track(rt, event == FLUX_POPUP_MOUSE_LEAVE ? -1.0f : x, y);
	if (on && event == FLUX_POPUP_MOUSE_DOWN) combo_bar_press(rt, x, y);
	if (on) combo_mouse_hover(rt, FLUX_POPUP_MOUSE_LEAVE, y);
	return on && event != FLUX_POPUP_MOUSE_LEAVE;
}

static void combo_mouse(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt || combo_bar_mouse(rt, event, x, y)) return;
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

static void combo_jump_selection(FluxComboRuntime *rt, int index) {
	int n = rt->model.item_count;
	if (n <= 0) return;
	if (index < 0) index = 0;
	if (index >= n) index = n - 1;
	if (rt->model.open) {
		rt->highlight = index;
		rt->kb_focus  = true;
		combo_ensure_visible(rt, index);
		combo_repaint_popup(rt);
	}
	if (index == rt->model.selected_index) return;
	rt->model.selected_index = index;
	combo_update_layout_text(rt);
	combo_repaint_owner(rt);
	combo_notify(rt, index);
}

static void combo_step_selection(FluxComboRuntime *rt, int delta) {
	combo_jump_selection(rt, rt->model.selected_index + delta);
}

static void combo_on_char(void *ctx, wchar_t ch);
static bool combo_searching(FluxComboRuntime const *rt);

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
	rt->kb_focus = !closing;
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
	if (key == FLUX_KEY_SPACE && combo_searching(rt)) {
		combo_on_char(rt, L' ');
		return true;
	}
	if (key == FLUX_KEY_ENTER || key == FLUX_KEY_SPACE) {
		combo_open(rt);
		rt->kb_focus = true;
		return true;
	}
	return false;
}

static bool combo_key_nav_open(FluxComboRuntime *rt, FluxKey key) {
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
	if (key == FLUX_KEY_PAGE_UP) {
		combo_step_selection(rt, -combo_page_rows(rt));
		return true;
	}
	if (key == FLUX_KEY_PAGE_DOWN) {
		combo_step_selection(rt, +combo_page_rows(rt));
		return true;
	}
	return false;
}

static bool combo_searching(FluxComboRuntime const *rt) {
	return rt->search_len > 0 && flux_anim_now() - rt->search_last <= 1000;
}

static bool combo_key_action_open(FluxComboRuntime *rt, FluxKey key) {
	if (key == FLUX_KEY_LEFT || key == FLUX_KEY_RIGHT) return true;
	if (key == FLUX_KEY_SPACE && combo_searching(rt)) {
		combo_on_char(rt, L' ');
		return true;
	}
	if (key == FLUX_KEY_ENTER && flux_key_held(FLUX_KEY_CONTROL) && rt->highlight == rt->model.selected_index) {
		rt->model.selected_index = -1;
		combo_update_layout_text(rt);
		combo_repaint_popup(rt);
		combo_notify(rt, -1);
		return true;
	}
	if (key == FLUX_KEY_SPACE || key == FLUX_KEY_ENTER) {
		combo_commit(rt, rt->highlight);
		return true;
	}
	if (key == FLUX_KEY_ESCAPE) {
		int previous = rt->selection_at_open;
		combo_close(rt);
		if (previous >= 0 && previous < rt->model.item_count) combo_jump_selection(rt, previous);
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

#define CB_SEARCH_WCAP 256

static bool combo_starts_with(char const *item, wchar_t const *prefix, int prefix_len) {
	wchar_t w [CB_SEARCH_WCAP];
	while (item && *item == ' ') item++;
	int n = item ? MultiByteToWideChar(CP_UTF8, 0, item, -1, w, CB_SEARCH_WCAP) - 1 : -1;
	if (n < 0 || prefix_len <= 0 || prefix_len > n) return false;
	DWORD flags = FIND_STARTSWITH
	            | NORM_IGNORECASE
	            | NORM_IGNOREKANATYPE
	            | NORM_IGNOREWIDTH
	            | NORM_LINGUISTIC_CASING
	            | LINGUISTIC_IGNOREDIACRITIC;
	return FindNLSStringEx(LOCALE_NAME_USER_DEFAULT, flags, w, n, prefix, prefix_len, NULL, NULL, NULL, 0) >= 0;
}

static bool combo_prefix_match(FluxComboRuntime const *rt, char const *s) {
	return combo_starts_with(s, rt->search, rt->search_len);
}

static void combo_search_push(FluxComboRuntime *rt, wchar_t ch) {
	DWORD now = flux_anim_now();
	if (now - rt->search_last > 1000 || rt->search_len >= ( int ) (sizeof(rt->search) / sizeof(rt->search [0])) - 1)
		rt->search_len = 0;
	rt->search [rt->search_len++] = ch;
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
	combo_jump_selection(rt, hit);
}

static char const *combo_editor_text(FluxComboRuntime const *rt) {
	FluxNodeData *nd = rt->editor == XENT_NODE_INVALID ? NULL : flux_store_find(rt->store, rt->editor);
	return nd && nd->component_data ? (( FluxTextBoxData * ) nd->component_data)->content : NULL;
}

static void combo_editor_write(FluxComboRuntime *rt, char const *text) {
	if (rt->editor == XENT_NODE_INVALID) return;
	char const *cur = combo_editor_text(rt);
	if (strcmp(cur ? cur : "", text ? text : "") == 0) return;
	rt->ignore_text = true;
	flux_prop_textbox_content(rt->store, rt->editor, text ? text : "");
	rt->ignore_text = false;
	combo_apply_min_width(rt);
}

static int combo_match_exact(FluxComboRuntime const *rt, char const *text) {
	if (!text || !text [0]) return -1;
	for (int i = 0; i < rt->model.item_count; i++)
		if (rt->model.items [i] && _stricmp(rt->model.items [i], text) == 0) return i;
	return -1;
}

static void combo_editor_changed(void *ctx, char const *text) {
	( void ) text;
	combo_apply_min_width(( FluxComboRuntime * ) ctx);
}

static void combo_editor_select(FluxComboRuntime *rt, int index, char const *custom);

static bool combo_same_text(char const *a, char const *b) {
	wchar_t wa [CB_SEARCH_WCAP], wb [CB_SEARCH_WCAP];
	int     na = MultiByteToWideChar(CP_UTF8, 0, a ? a : "", -1, wa, CB_SEARCH_WCAP);
	int     nb = MultiByteToWideChar(CP_UTF8, 0, b ? b : "", -1, wb, CB_SEARCH_WCAP);
	return na > 0 && na == nb && _wcsicmp(wa, wb) == 0;
}

static int combo_editor_find(FluxComboRuntime const *rt, char const *text, bool backspace) {
	wchar_t w [CB_SEARCH_WCAP];
	int     n = MultiByteToWideChar(CP_UTF8, 0, text, -1, w, CB_SEARCH_WCAP) - 1;
	for (int i = 0; i < rt->model.item_count; i++) {
		char const *item = rt->model.items [i];
		if (backspace ? combo_same_text(item, text) : combo_starts_with(item, w, n)) return i;
	}
	return -1;
}

static void combo_editor_complete(FluxComboRuntime *rt, int index) {
	FluxNodeData    *nd = flux_store_find(rt->store, rt->editor);
	FluxTextBoxData *td = nd ? ( FluxTextBoxData * ) nd->component_data : NULL;
	char const      *s  = rt->model.items [index];
	if (!td || !s || combo_same_text(s, td->content)) return;
	uint32_t caret = td->cursor_position;
	combo_editor_write(rt, s);
	td->selection_start = caret;
	td->selection_end   = ( uint32_t ) strlen(s);
	td->cursor_position = td->selection_end;
}

static void combo_editor_search(FluxComboRuntime *rt, wchar_t ch) {
	char const *text = combo_editor_text(rt);
	if (combo_same_text(text, rt->edit_search)) return;
	int found         = text && text [0] ? combo_editor_find(rt, text, ch == L'\b') : -1;
	rt->search_set    = true;
	rt->search_result = found;
	flux_str_replace(&rt->edit_search, text);
	if (found >= 0 && ch != L'\b') {
		combo_editor_complete(rt, found);
		flux_str_replace(&rt->edit_search, rt->model.items [found]);
	}
	if (!rt->model.open) return;
	rt->highlight = found;
	if (found >= 0) combo_ensure_visible(rt, found);
	combo_repaint_popup(rt);
}

static void combo_editor_on_char(void *ctx, wchar_t ch) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (rt->editor_on_char) rt->editor_on_char(rt->editor_on_char_ctx, ch);
	combo_editor_search(rt, ch);
}

static int combo_open_highlight(FluxComboRuntime const *rt) {
	if (!rt->model.editable) return rt->model.selected_index;
	char const *text = combo_editor_text(rt);
	if (!text || !text [0]) return rt->model.selected_index;
	return combo_editor_find(rt, text, false);
}

static void combo_route_submit(FluxComboRuntime *rt, char const *text) {
	void  (*notify)(void *, char const *) = rt->model.on_submit;
	void *notify_ctx                      = rt->model.on_select_ctx;
	flux_input_route_event(
	  flux_store_host(rt->store), rt->node, XTK_EVENT_SUBMIT_STRING,
	  (XtkEventPayload) {
		.kind     = XTK_EVENT_PAYLOAD_STRING,
		.u.string = {text, ( uint32_t ) strlen(text)},
	},
	  XTK_EVENT_CAUSE_USER
	);
	if (notify) notify(notify_ctx, text);
}

static void combo_editor_selectall(FluxComboRuntime *rt) {
	FluxNodeData    *nd = flux_store_find(rt->store, rt->editor);
	FluxTextBoxData *td = nd ? ( FluxTextBoxData * ) nd->component_data : NULL;
	if (!td) return;
	td->selection_start = 0;
	td->selection_end   = td->content ? ( uint32_t ) strlen(td->content) : 0u;
	td->cursor_position = td->selection_end;
}

static void combo_editor_mark_restore(FluxComboRuntime *rt) {
	if (rt->restore_set) return;
	rt->restore_set   = true;
	rt->restore_index = rt->model.selected_index;
	flux_str_replace(&rt->restore_custom, rt->custom_value);
}

static void combo_editor_select(FluxComboRuntime *rt, int index, char const *custom) {
	bool changed = index != rt->model.selected_index;
	if (changed) combo_editor_mark_restore(rt);
	combo_set_selected(rt, index);
	flux_str_replace(&rt->custom_value, index < 0 ? custom : NULL);
	combo_update_layout_text(rt);
	if (changed) combo_notify(rt, index);
}

static bool combo_text_blank(char const *s) {
	while (s && (*s == ' ' || *s == '\t')) s++;
	return !s || !*s;
}

static void combo_editor_commit_text(FluxComboRuntime *rt) {
	char *text = flux_str_dup(combo_editor_text(rt));
	if (text && !combo_text_blank(text) && !combo_same_text(text, rt->custom_value)) {
		combo_route_submit(rt, text);
		int exact = combo_match_exact(rt, text);
		combo_editor_select(rt, exact, exact < 0 ? text : NULL);
	}
	flux_str_free(text);
}

static void combo_editor_commit(FluxComboRuntime *rt, bool restore) {
	if (restore && rt->restore_set) combo_editor_select(rt, rt->restore_index, rt->restore_custom);
	if (!restore && !rt->search_set) combo_editor_search(rt, L' ');
	if (!restore && rt->search_result >= 0) combo_editor_select(rt, rt->search_result, NULL);
	else if (!restore) combo_editor_commit_text(rt);
	int         i    = rt->model.selected_index;
	char const *show = i >= 0 && i < rt->model.item_count ? rt->model.items [i] : rt->custom_value;
	combo_editor_write(rt, show ? show : "");
	combo_editor_selectall(rt);
	rt->search_set    = false;
	rt->search_result = -1;
	rt->restore_set   = false;
	flux_str_replace(&rt->edit_search, NULL);
}

static void combo_editor_step(FluxComboRuntime *rt, int delta) {
	int n = rt->model.item_count;
	if (n <= 0) return;
	int i = rt->model.selected_index < 0 ? (delta > 0 ? 0 : n - 1) : rt->model.selected_index + delta;
	i     = i < 0 ? 0 : (i >= n ? n - 1 : i);
	combo_editor_select(rt, i, NULL);
	combo_editor_write(rt, rt->model.items [i]);
	combo_editor_selectall(rt);
	rt->highlight = i;
	rt->kb_focus  = true;
	combo_ensure_visible(rt, i);
	combo_repaint_popup(rt);
}

static bool combo_editor_key_open(FluxComboRuntime *rt, FluxKey key) {
	switch (key) {
	case FLUX_KEY_DOWN      : combo_editor_step(rt, +1); return true;
	case FLUX_KEY_UP        : combo_editor_step(rt, -1); return true;
	case FLUX_KEY_PAGE_UP   : combo_editor_step(rt, -combo_page_rows(rt)); return true;
	case FLUX_KEY_PAGE_DOWN : combo_editor_step(rt, +combo_page_rows(rt)); return true;
	case FLUX_KEY_ESCAPE :
		combo_close(rt);
		combo_editor_commit(rt, true);
		return true;
	case FLUX_KEY_ENTER :
		combo_close(rt);
		combo_editor_commit(rt, false);
		return true;
	case FLUX_KEY_TAB : combo_close(rt); return false;
	default           : return false;
	}
}

static bool combo_editor_key_closed(FluxComboRuntime *rt, FluxKey key) {
	switch (key) {
	case FLUX_KEY_ENTER  : combo_editor_commit(rt, false); return true;
	case FLUX_KEY_ESCAPE : combo_editor_commit(rt, true); return true;
	case FLUX_KEY_UP     :
	case FLUX_KEY_DOWN :
		combo_editor_mark_restore(rt);
		combo_open(rt);
		return true;
	default : return false;
	}
}

static bool combo_editor_handles(FluxComboRuntime *rt, FluxKey key) {
	if (combo_toggle_popup_key(rt, key, rt->model.open)) return true;
	return rt->model.open ? combo_editor_key_open(rt, key) : combo_editor_key_closed(rt, key);
}

static bool combo_editor_on_key(void *ctx, FluxKey key, bool down) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (down && combo_editor_handles(rt, key)) return true;
	return rt->editor_on_key ? rt->editor_on_key(rt->editor_on_key_ctx, key, down) : false;
}

static void combo_editor_on_down(void *ctx, float x, float y, int clicks) {
	FluxComboRuntime *rt   = ( FluxComboRuntime * ) ctx;
	XentRect          rect = {0};
	xent_layout_rect(rt->ctx, rt->editor, &rect);
	rt->overlay_down = x >= rect.w - FLUX_COMBO_GLYPH_COL;
	if (!rt->overlay_down && rt->editor_on_down) rt->editor_on_down(rt->editor_on_down_ctx, x, y, clicks);
}

static void combo_editor_on_click(void *ctx) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (rt->overlay_down) combo_toggle(rt);
	else if (rt->editor_on_click) rt->editor_on_click(rt->editor_on_click_ctx);
	rt->overlay_down = false;
}

static void combo_editor_expand(FluxComboRuntime *rt, bool on) {
	if (rt->editor == XENT_NODE_INVALID) return;
	rt->model.edit_active = on;
	xent_setgrow(rt->ctx, rt->editor, on ? 1.0f : 0.0f);
	xent_setbasis(rt->ctx, rt->editor, 0.0f);
	xent_setsize(rt->ctx, rt->editor, on ? (XentSize) {NAN, NAN} : (XentSize) {0.0f, 0.0f});
	xent_setminsize(rt->ctx, rt->editor, (XentSize) {0.0f, 0.0f});
	xent_set_focusable(rt->ctx, rt->editor, on);
	xent_set_focusable(rt->ctx, rt->node, !on);
}

static void combo_editor_on_blur(void *ctx) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (rt->editor_on_blur) rt->editor_on_blur(rt->editor_on_blur_ctx);
	if (rt->model.open) combo_close(rt);
	combo_editor_commit(rt, false);
	combo_editor_expand(rt, false);
}

static void combo_on_focus(void *ctx) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt->model.editable || rt->editor == XENT_NODE_INVALID || !rt->input) return;
	combo_editor_expand(rt, true);
	flux_input_setfocus(rt->input, rt->editor, false);
}

static void combo_editor_on_focus(void *ctx) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (rt->editor_on_focus) rt->editor_on_focus(rt->editor_on_focus_ctx);
	combo_editor_expand(rt, true);
	combo_editor_selectall(rt);
	combo_editor_search(rt, L' ');
	FluxNodeData *nd = flux_store_find(rt->store, rt->node);
	if (nd && nd->state.pressed && nd->state.pointer_type == FLUX_POINTER_TOUCH && !rt->model.open) combo_open(rt);
}

static void combo_editor_create(FluxComboRuntime *rt) {
	if (rt->editor != XENT_NODE_INVALID || !rt->app) return;
	rt->editor = flux_app_create_textbox(&(FluxAppTextBoxDesc) {
	  rt->app, rt->node, rt->model.placeholder, combo_editor_changed, rt});
	if (rt->editor == XENT_NODE_INVALID) return;
	FluxNodeData *tnd = flux_store_find(rt->store, rt->editor);
	if (!tnd) return;
	FluxTextBoxData *td = ( FluxTextBoxData * ) tnd->component_data;
	td->combo_editor    = true;
	rt->model.edit_text = &td->content;
	combo_editor_expand(rt, false);
	rt->editor_on_char                = tnd->behavior.on_char;
	rt->editor_on_char_ctx            = tnd->behavior.on_char_ctx;
	tnd->behavior.on_char             = combo_editor_on_char;
	tnd->behavior.on_char_ctx         = rt;
	rt->editor_on_blur                = tnd->behavior.on_blur;
	rt->editor_on_blur_ctx            = tnd->behavior.on_blur_ctx;
	tnd->behavior.on_blur             = combo_editor_on_blur;
	tnd->behavior.on_blur_ctx         = rt;
	rt->editor_on_focus               = tnd->behavior.on_focus;
	rt->editor_on_focus_ctx           = tnd->behavior.on_focus_ctx;
	tnd->behavior.on_focus            = combo_editor_on_focus;
	tnd->behavior.on_focus_ctx        = rt;
	rt->editor_on_key                 = tnd->behavior.on_key;
	rt->editor_on_key_ctx             = tnd->behavior.on_key_ctx;
	tnd->behavior.on_key              = combo_editor_on_key;
	tnd->behavior.on_key_ctx          = rt;
	rt->editor_on_down                = tnd->behavior.on_pointer_down;
	rt->editor_on_down_ctx            = tnd->behavior.on_pointer_down_ctx;
	tnd->behavior.on_pointer_down     = combo_editor_on_down;
	tnd->behavior.on_pointer_down_ctx = rt;
	rt->editor_on_click               = tnd->behavior.on_click;
	rt->editor_on_click_ctx           = tnd->behavior.on_click_ctx;
	tnd->behavior.on_click            = combo_editor_on_click;
	tnd->behavior.on_click_ctx        = rt;
}

static void combo_layout_editable(FluxComboRuntime *rt, bool on) {
	xent_setproto(rt->ctx, rt->node, XENT_PROTOCOL_FLEX);
	xent_setflexdir(rt->ctx, rt->node, XENT_FLEX_ROW);
	xent_setitems(rt->ctx, rt->node, XENT_FLEX_ALIGN_STRETCH);
	xent_setp(rt->ctx, rt->node, on ? (XentInsets) {0} : (XentInsets) {12.0f, 0.0f, FLUX_COMBO_GLYPH_COL, 0.0f});
	xent_set_focusable(rt->ctx, rt->node, true);
}

static void combo_set_editable(FluxComboRuntime *rt, bool on) {
	if (rt->model.editable == on) return;
	rt->model.editable = on;
	if (!on && rt->editor != XENT_NODE_INVALID) {
		flux_tree_drop(rt->store, rt->editor);
		rt->editor            = XENT_NODE_INVALID;
		rt->model.edit_text   = NULL;
		rt->model.edit_active = false;
	}
	combo_layout_editable(rt, on);
	if (on) combo_editor_create(rt);
	combo_update_layout_text(rt);
	combo_apply_min_width(rt);
	if (on && rt->content) combo_editor_write(rt, rt->content);
}

static void combo_destroy(void *component_data) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) component_data;
	if (!rt) return;
	for (int i = 0; i < rt->model.item_count; i++) flux_str_free(rt->model.items [i]);
	free(( void * ) rt->model.items);
	flux_str_free(rt->model.placeholder);
	flux_str_free(rt->content);
	flux_str_free(rt->edit_search);
	flux_str_free(rt->custom_value);
	flux_str_free(rt->restore_custom);
	if (rt->popup) flux_popup_destroy(rt->popup);
	free(rt);
}

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
	if (index == rt->model.selected_index) return;
	combo_set_selected(rt, index);
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
	if (rt->editor != XENT_NODE_INVALID) flux_prop_textbox_placeholder(store, rt->editor, placeholder);
}

void flux_prop_combo_editable(FluxStore *store, XentNodeId id, bool editable) {
	FluxComboRuntime *rt = combo_runtime(store, id);
	if (rt) combo_set_editable(rt, editable);
}

void flux_prop_combo_text(FluxStore *store, XentNodeId id, char const *text) {
	FluxComboRuntime *rt = combo_runtime(store, id);
	if (!rt) return;
	flux_str_replace(&rt->content, text);
	if (rt->model.editable) combo_editor_write(rt, text);
}

char const *flux_combo_text(FluxStore *store, XentNodeId id) {
	FluxComboRuntime *rt = combo_runtime(store, id);
	if (!rt) return NULL;
	return rt->model.editable ? combo_editor_text(rt) : combo_selected_text(rt);
}

static void combo_apply_min_width(FluxComboRuntime *rt) {
	if (!rt->text || rt->node == XENT_NODE_INVALID) return;
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_FONT_SIZE_DEFAULT;
	ts.font_weight = FLUX_FONT_REGULAR;
	float w        = 64.0f;
	if (rt->model.editable) {
		char const *shown = combo_editor_text(rt);
		if (!shown || !shown [0]) shown = rt->model.placeholder;
		if (shown && shown [0])
			w = flux_maxf(w, flux_text_measure(rt->text, shown, &ts, 0).w + 12.0f + FLUX_COMBO_GLYPH_COL);
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
	rt->model.on_submit      = info->on_submit;
	rt->model.on_select_ctx  = info->userdata;
	rt->store                = info->store;
	rt->node                 = node;
	rt->ctx                  = info->ctx;
	rt->window               = info->window;
	rt->text                 = info->text;
	rt->theme                = info->theme;
	rt->highlight            = -1;
	rt->hover_index          = -1;
	rt->bar_hover_x          = -1.0f;
	rt->bar_hover_y          = -1.0f;
	rt->pressed_index        = -1;
	rt->app                  = info->app;
	rt->input                = info->input;
	rt->editor               = XENT_NODE_INVALID;

	rt->popup                = flux_popup_create(info->window);
	if (rt->popup) {
		flux_popup_setdismissoutside(rt->popup, true);
		flux_popup_setanim(rt->popup, FLUX_POPUP_ANIM_SPLIT);
		flux_popup_onrecord(rt->popup, combo_record, rt, rt->text);
		flux_popup_setshadow(rt->popup, true, CB_CORNER);
		flux_popup_onmouse(rt->popup, combo_mouse, rt);
		flux_popup_ondismiss(rt->popup, combo_dismissed, rt);
	}

	nd->component_data         = &rt->model;
	nd->destroy_component_data = combo_destroy;
	nd->behavior.on_click      = combo_on_click;
	nd->behavior.on_focus      = combo_on_focus;
	nd->behavior.on_focus_ctx  = rt;
	nd->behavior.on_click_ctx  = rt;
	nd->behavior.on_key        = combo_on_key;
	nd->behavior.on_key_ctx    = rt;
	nd->behavior.on_char       = combo_on_char;
	nd->behavior.on_char_ctx   = rt;

	xent_setfontsize(info->ctx, node, FLUX_FONT_SIZE_DEFAULT);
	xent_set_focusable(info->ctx, node, true);
	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_BUTTON);
	xent_setsize(info->ctx, node, (XentSize) {NAN, 32.0f});
	xent_setminsize(info->ctx, node, (XentSize) {80.0f, 32.0f});
	xent_setp(info->ctx, node, (XentInsets) {12.0f, 0, FLUX_COMBO_GLYPH_COL, 0});
	combo_update_layout_text(rt);
	combo_apply_min_width(rt);
	if (info->editable) combo_set_editable(rt, true);
	return node;
}

bool flux_combo_handle_wheel(FluxStore *store, XentNodeId id, float delta) {
	FluxComboRuntime *rt      = combo_runtime(store, id);
	FluxNodeData     *nd      = flux_store_find(store, id);
	bool              focused = rt && nd && (nd->state.focused || (rt->model.edit_active && rt->model.editable));
	if (!focused || !xent_sem_enabled(rt->ctx, id)) return false;
	if (!rt->model.open && delta != 0.0f) combo_step_selection(rt, delta < 0.0f ? +1 : -1);
	return true;
}
