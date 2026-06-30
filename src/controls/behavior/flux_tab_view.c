#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"
#include "render/flux_fluent.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_window.h"
#include "fluxent/controls/flux_scroll_data.h"
#include "fluxent/controls/flux_tab_view_data.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <windows.h>

/* WinUI TabView (TabView.cpp / TabViewItem.cpp / TabView.xaml). Strip row =
 * [header][scroll-dec][scrollable tab host][scroll-inc][add][footer] over a
 * content host; the selected tab owns the attached content subtree. Tabs share
 * the strip equally by default (TabWidthMode::Equal, clamped [100,240]) and
 * overflow into repeat-button scrolling (50px per click). Closing collapses the
 * tab's width on a shared timer, then detaches it; widths recompute only after
 * the pointer leaves the strip so the next close button lands under the cursor
 * (TabView::OnTabStripPointerExited). Tabs drag-reorder with the pointer
 * (CanReorderTabs default), close on middle-click release, and follow WinUI
 * keyboarding: arrows move focus, Ctrl+Tab cycles selection, Ctrl+F4 closes. */

#define TAB_CLOSE_MS        166.0f
#define TAB_WIDTH_ANIM_MS   250.0f /* AddDeleteThemeTransition feel */
#define TAB_SLIDE_MS        250.0f /* ReorderThemeTransition feel */
#define TAB_SCROLL_ANIM_MS  250.0f /* ScrollViewer::ChangeView smooth scroll */
#define TAB_REPEAT_DELAY_MS 400
#define TAB_REPEAT_INTERVAL 100
#define TAB_SCROLL_RESERVE  (2.0f * (FLUX_TAB_SCROLL_W + FLUX_TAB_SCROLL_PAD_OUT + FLUX_TAB_SCROLL_PAD_IN))

static FluxTabViewData *tv_data(FluxNodeStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_node_store_get(store, node);
	if (!nd || nd->component_type != FLUX_CONTROL_TAB_VIEW) return NULL;
	return ( FluxTabViewData * ) nd->component_data;
}

static void tv_repaint(FluxTabViewData *tv) {
	HWND h = flux_window_hwnd(tv->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static bool tv_slot_open(FluxTabViewData const *tv, int slot) {
	return !tv->tabs [slot].closed && tv->tabs [slot].kind == FLUX_TAB_KIND_TAB;
}

static int tv_order_pos(FluxTabViewData const *tv, int slot) {
	for (int p = 0; p < tv->count; p++)
		if (tv->order [p] == slot) return p;
	return -1;
}

/* Next open tab slot scanning the display order from @p pos (exclusive). */
static int tv_open_from(FluxTabViewData const *tv, int pos, int dir) {
	for (int p = pos + dir; p >= 0 && p < tv->count; p += dir)
		if (tv_slot_open(tv, tv->order [p])) return tv->order [p];
	return -1;
}

static FluxScrollData *tv_scroll(FluxTabViewData const *tv) {
	FluxNodeData *nd = flux_node_store_get(tv->store, tv->tabs_host);
	return nd ? ( FluxScrollData * ) nd->component_data : NULL;
}

static void tv_attach_content(FluxTabViewData *tv, int idx, bool attach) {
	if (idx < 0 || idx >= tv->count) return;
	XentNodeId c = tv->tabs [idx].content;
	if (c == XENT_NODE_INVALID) return;
	if (attach) xent_append_child(tv->ctx, tv->content, c);
	else xent_remove_child(tv->ctx, tv->content, c);
}

static void tv_update_widths(FluxTabViewData *tv, bool animate);
static void tv_sync_close(FluxTabViewData *tv, FluxTabViewItem *it);
static void tv_anim_start(FluxTabViewData *tv);
static void tv_scroll_to(FluxTabViewData *tv, float target);
static void tv_animate_width(FluxTabViewData *tv, FluxTabViewItem *it, bool animate);
static void tv_sync_scroll_enabled(FluxTabViewData *tv);

/* Keep the selected tab inside the scrolled host viewport (BringIntoView). */
static void tv_scroll_into_view(FluxTabViewData *tv, int slot) {
	FluxScrollData *sd = tv_scroll(tv);
	XentRect        hr = {0};
	if (!sd || !tv->scroll_visible) return;
	if (!xent_get_layout_rect(tv->ctx, tv->tabs_host, &hr) || hr.w <= 0.0f) return;

	float x0 = 0.0f;
	for (int p = 0; p < tv_order_pos(tv, slot); p++)
		if (tv_slot_open(tv, tv->order [p])) x0 += tv->tabs [tv->order [p]].cur_w;
	float x1 = x0 + tv->tabs [slot].cur_w; /* target widths: where tabs settle */
	if (x0 < sd->scroll_x) tv_scroll_to(tv, x0);
	else if (x1 > sd->scroll_x + hr.w) tv_scroll_to(tv, x1 - hr.w);
}

static void tv_select(FluxTabViewData *tv, int idx) {
	if (idx < 0 || idx >= tv->count || !tv_slot_open(tv, idx)) return;
	if (idx == tv->selected) return;
	if (tv->selected >= 0) tv_attach_content(tv, tv->selected, false);
	int prev     = tv->selected;
	tv->selected = idx;
	tv_attach_content(tv, idx, true);
	xent_set_semantic_selected(tv->ctx, tv->tabs [idx].tab_node, true);
	if (prev >= 0) tv_sync_close(tv, &tv->tabs [prev]);
	tv_sync_close(tv, &tv->tabs [idx]);
	if (tv->width_mode == FLUX_TAB_WIDTH_COMPACT) tv_update_widths(tv, true);
	tv_scroll_into_view(tv, idx);
	if (tv->on_selection_changed) tv->on_selection_changed(tv->cb_ctx, idx);
	tv_repaint(tv);
}

static float tv_measure_width(FluxTabViewData *tv, char const *icon, char const *label, bool closable) {
	float w = FLUX_TAB_PAD_L
	        + (closable ? FLUX_TAB_CLOSE_MARGIN_L + FLUX_TAB_CLOSE_W + FLUX_TAB_PAD_R : FLUX_TAB_PAD_R_NOCLOSE);
	if (icon) w += FLUX_TAB_ICON + FLUX_TAB_ICON_GAP;
	if (label && tv->text) {
		FluxTextStyle ts  = {0};
		ts.font_size      = FLUX_TAB_FONT;
		ts.font_weight    = FLUX_FONT_SEMI_BOLD; /* widest weight, avoids reflow on select */
		w                += flux_text_measure(tv->text, label, &ts, FLT_MAX).w;
	}
	return flux_clampf(w, FLUX_TAB_MIN_W, FLUX_TAB_MAX_W);
}

static float tv_node_width(FluxTabViewData *tv, XentNodeId node) {
	XentRect r = {0};
	if (node == XENT_NODE_INVALID || !xent_get_layout_rect(tv->ctx, node, &r)) return 0.0f;
	return r.w;
}

/* Width available to tabs when no overflow buttons are shown, from the STRIP's
 * laid-out rect (stable geometry: it never depends on tab widths or on the
 * scroll-button visibility, so the overflow decision cannot feed back into
 * itself). -1 before the first layout lands. */
static float tv_avail_base(FluxTabViewData *tv) {
	XentRect sr = {0};
	if (!xent_get_layout_rect(tv->ctx, tv->strip, &sr) || sr.w <= 0.0f) return -1.0f;
	float base = sr.w - tv_node_width(tv, tv->strip_header) - tv_node_width(tv, tv->strip_footer);
	if (tv->add_node != XENT_NODE_INVALID) base -= FLUX_TAB_ADD_PAD_L + FLUX_TAB_ADD_W;
	return base;
}

/* Per-mode target width for one open tab. */
static float tv_mode_width(FluxTabViewData const *tv, FluxTabViewItem const *it, float equal_w) {
	if (tv->width_mode == FLUX_TAB_WIDTH_EQUAL) return equal_w;
	if (tv->width_mode == FLUX_TAB_WIDTH_COMPACT && tv->selected != it->index) return FLUX_TAB_COMPACT_W;
	return it->width;
}

/* Tween a tab's displayed width toward cur_w, or snap when the caller follows
 * live geometry (resize / first settle) or the delta is negligible. */
static void tv_animate_width(FluxTabViewData *tv, FluxTabViewItem *it, bool animate) {
	if (!animate || fabsf(it->cur_w - it->disp_w) < 0.5f) {
		it->disp_w = it->cur_w;
		it->w_anim = false;
		xent_set_size(tv->ctx, it->tab_node, (XentSize) {it->disp_w, FLUX_TAB_MIN_H});
		return;
	}
	it->w_from  = it->disp_w;
	it->w_start = GetTickCount();
	it->w_anim  = true;
	tv_anim_start(tv);
}

static bool tv_node_hot(FluxTabViewData *tv, XentNodeId node) {
	FluxNodeData *nd = flux_node_store_get(tv->store, node);
	return nd && (nd->state.hovered || nd->state.pressed);
}

/* CloseButtonOverlayMode: Auto/Always keep the button on every closable tab
 * (TabViewItem::UpdateCloseButton's default branch); OnPointerOver gates it on
 * selection or hover (of the tab or of the button itself). */
static void tv_sync_close(FluxTabViewData *tv, FluxTabViewItem *it) {
	if (it->close_node == XENT_NODE_INVALID) return;
	bool show = it->closable && !it->closing;
	if (show && tv->close_overlay == FLUX_TAB_CLOSE_OVERLAY_ON_HOVER)
		show = tv->selected == it->index || tv_node_hot(tv, it->tab_node) || tv_node_hot(tv, it->close_node);
	xent_set_size(tv->ctx, it->close_node, (XentSize) {show ? FLUX_TAB_CLOSE_W : 0.0f, show ? FLUX_TAB_CLOSE_H : 0.0f});
}

static void tv_hover_changed(void *ctx, bool hovered) {
	( void ) hovered;
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	tv_sync_close(it->tv, it);
	tv_repaint(it->tv);
}

/* WinUI disables the repeat buttons at the ends (threshold 0.1). The tween
 * target is the offset the view is headed to, so the state flips immediately. */
static void tv_sync_scroll_enabled(FluxTabViewData *tv) {
	if (!tv->scroll_visible) return;
	FluxScrollData *sd = tv_scroll(tv);
	if (!sd) return;
	float avail = tv_avail_base(tv) - TAB_SCROLL_RESERVE;
	float max_x = flux_maxf(0.0f, sd->content_w - avail);
	float at    = tv->scroll_anim ? tv->scroll_target : sd->scroll_x;
	xent_set_semantic_enabled(tv->ctx, tv->scroll_dec, at > 0.1f);
	xent_set_semantic_enabled(tv->ctx, tv->scroll_inc, at < max_x - 0.1f);
}

static XentInsets tv_scroll_btn_margin(bool show, float pad_start, float pad_end) {
	if (!show) return (XentInsets) {0};
	return (XentInsets) {pad_start, 0.0f, pad_end, FLUX_TAB_SCROLL_PAD_B};
}

static void tv_show_scroll_buttons(FluxTabViewData *tv, bool show) {
	if (tv->scroll_visible == show) return;
	tv->scroll_visible = show;
	float w            = show ? FLUX_TAB_SCROLL_W : 0.0f;
	float h            = show ? FLUX_TAB_SCROLL_H : 0.0f;
	xent_set_size(tv->ctx, tv->scroll_dec, (XentSize) {w, h});
	xent_set_size(tv->ctx, tv->scroll_inc, (XentSize) {w, h});
	xent_set_margin(tv->ctx, tv->scroll_dec, tv_scroll_btn_margin(show, FLUX_TAB_SCROLL_PAD_OUT, FLUX_TAB_SCROLL_PAD_IN));
	xent_set_margin(tv->ctx, tv->scroll_inc, tv_scroll_btn_margin(show, FLUX_TAB_SCROLL_PAD_IN, FLUX_TAB_SCROLL_PAD_OUT));
}

/* Apply per-mode tab widths, toggle overflow scroll buttons, sync scroll range. */
/* Sum of per-mode target widths if tabs were sized against @p avail. */
static float tv_mode_total(FluxTabViewData *tv, int n, float avail) {
	float equal_w = flux_clampf(avail / ( float ) n, FLUX_TAB_MIN_W, FLUX_TAB_MAX_W);
	float total   = 0.0f;
	for (int i = 0; i < tv->count; i++)
		if (tv_slot_open(tv, i)) total += tv_mode_width(tv, &tv->tabs [i], equal_w);
	return total;
}

/* Apply per-tab target widths and return total displayed width. */
static float tv_apply_tab_widths(FluxTabViewData *tv, float equal_w, bool animate) {
	float total = 0.0f;
	for (int i = 0; i < tv->count; i++) {
		FluxTabViewItem *it = &tv->tabs [i];
		if (!tv_slot_open(tv, i)) continue;
		if (!it->closing) {
			it->cur_w = tv_mode_width(tv, it, equal_w);
			tv_animate_width(tv, it, animate);
		}
		total += it->cur_w;
	}
	return total;
}

/* Clamp scroll data extents after a width recompute. */
static void tv_clamp_scroll(FluxTabViewData *tv, float total, float avail) {
	FluxScrollData *sd = tv_scroll(tv);
	if (!sd) return;
	float max_x   = flux_maxf(0.0f, total - avail);
	sd->content_w = total;
	sd->content_h = FLUX_TAB_MIN_H;
	sd->scroll_x  = flux_clampf(sd->scroll_x, 0.0f, max_x);
	if (tv->scroll_anim) tv->scroll_target = flux_clampf(tv->scroll_target, 0.0f, max_x);
}

static void tv_update_widths(FluxTabViewData *tv, bool animate) {
	int n = 0;
	for (int i = 0; i < tv->count; i++)
		if (tv_slot_open(tv, i)) n++;
	if (n == 0) return;

	float base = tv_avail_base(tv);
	if (base <= 0.0f) {
		tv->widths_valid = false; /* no layout yet; the shared timer retries */
		tv_anim_start(tv);
		return;
	}
	/* Overflow is decided against the button-less width only, so showing the
	 * buttons can never flip the decision back (no oscillation). */
	bool  overflow = tv_mode_total(tv, n, base) > base + 0.5f;
	float avail    = base - (overflow ? TAB_SCROLL_RESERVE : 0.0f);
	float equal_w  = flux_clampf(avail / ( float ) n, FLUX_TAB_MIN_W, FLUX_TAB_MAX_W);
	float total    = tv_apply_tab_widths(tv, equal_w, animate);

	tv_show_scroll_buttons(tv, overflow);
	tv_clamp_scroll(tv, total, avail);
	tv_sync_scroll_enabled(tv);
}

/* WinUI defers the recompute while the pointer is over the strip, so closing a
 * row of tabs keeps each next close button under the cursor. */
static bool tv_strip_hovered(FluxTabViewData *tv) {
	XentNodeId nodes [4] = {tv->strip, tv->tabs_host, tv->add_node, XENT_NODE_INVALID};
	for (int i = 0; i < 3; i++) {
		FluxNodeData *nd = flux_node_store_get(tv->store, nodes [i]);
		if (nd && nd->state.hovered) return true;
	}
	for (int i = 0; i < tv->count; i++) {
		if (!tv_slot_open(tv, i)) continue;
		if (tv_node_hot(tv, tv->tabs [i].tab_node) || tv_node_hot(tv, tv->tabs [i].close_node)) return true;
	}
	return false;
}

static void tv_anim_remove(FluxTabViewData *tv) {
	tv->anim_active = false;
	flux_anim_unregister(tv);
}

static bool tv_step(void *ctx, unsigned long now);

static void tv_anim_start(FluxTabViewData *tv) {
	tv->anim_active = true;
	flux_anim_register(tv, tv_step);
}

static void tv_finalize_close(FluxTabViewData *tv, int idx) {
	FluxTabViewItem *it = &tv->tabs [idx];
	it->closing         = false;
	it->closed          = true;
	if (tv->selected == idx) {
		tv_attach_content(tv, idx, false);
		tv->selected = -1;
	}
	xent_remove_child(tv->ctx, tv->tabs_host, it->tab_node);
	xent_destroy_node(tv->ctx, it->tab_node);
	if (it->content != XENT_NODE_INVALID) xent_destroy_node(tv->ctx, it->content);
	it->tab_node = XENT_NODE_INVALID;
	it->content  = XENT_NODE_INVALID;
	flux_str_free(it->label);
	flux_str_free(it->icon_name);
	it->label     = NULL;
	it->icon_name = NULL;

	if (tv->on_tab_closed) tv->on_tab_closed(tv->cb_ctx, idx);
	if (tv->selected < 0) {
		int pos  = tv_order_pos(tv, idx);
		int next = tv_open_from(tv, pos, +1);
		if (next < 0) next = tv_open_from(tv, pos, -1);
		if (next >= 0) tv_select(tv, next);
	}
	tv->width_update_pending = true; /* applied immediately unless hovered (see tick) */
}

static void tv_begin_close(FluxTabViewData *tv, int idx) {
	if (idx < 0 || idx >= tv->count || !tv_slot_open(tv, idx) || tv->tabs [idx].closing) return;
	if (!tv->tabs [idx].closable) return;
	if (tv->on_close_requested && !tv->on_close_requested(tv->cb_ctx, idx)) return;
	XentRect r = {0};
	xent_get_layout_rect(tv->ctx, tv->tabs [idx].tab_node, &r);
	tv->tabs [idx].w_anim      = false;
	tv->tabs [idx].close_w0    = r.w > 0.0f ? r.w : tv->tabs [idx].disp_w;
	tv->tabs [idx].close_start = GetTickCount();
	tv->tabs [idx].closing     = true;
	tv_sync_close(tv, &tv->tabs [idx]);
	tv_anim_start(tv);
}

/* Animated offset change (WinUI ChangeView). Consecutive steps retarget the
 * running tween so held repeat-buttons glide continuously. */
static void tv_scroll_to(FluxTabViewData *tv, float target) {
	FluxScrollData *sd = tv_scroll(tv);
	if (!sd) return;
	float avail       = tv_avail_base(tv) - (tv->scroll_visible ? TAB_SCROLL_RESERVE : 0.0f);
	tv->scroll_target = flux_clampf(target, 0.0f, flux_maxf(0.0f, sd->content_w - avail));
	tv->scroll_from   = sd->scroll_x;
	tv->scroll_start  = GetTickCount();
	tv->scroll_anim   = true;
	tv_sync_scroll_enabled(tv);
	tv_anim_start(tv);
}

static void tv_scroll_by(FluxTabViewData *tv, float dx) {
	FluxScrollData *sd = tv_scroll(tv);
	if (!sd) return;
	tv_scroll_to(tv, (tv->scroll_anim ? tv->scroll_target : sd->scroll_x) + dx);
}

static void tv_scroll_btn_down(void *ctx, float x, float y, int clicks) {
	( void ) x;
	( void ) y;
	( void ) clicks;
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	FluxTabViewData *tv = it->tv;
	int              d  = (it->kind == FLUX_TAB_KIND_SCROLL_DEC) ? -1 : +1;
	tv_scroll_by(tv, ( float ) d * FLUX_TAB_SCROLL_AMOUNT);
	tv->scroll_held = d;
	tv->scroll_next = GetTickCount() + TAB_REPEAT_DELAY_MS;
	tv_anim_start(tv);
}

static void tv_scroll_btn_release(void *ctx) {
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	it->tv->scroll_held = 0;
}

static void tv_rebuild_tab_order(FluxTabViewData *tv) {
	for (int p = 0; p < tv->count; p++) {
		int slot = tv->order [p];
		if (tv->tabs [slot].closed) continue;
		xent_remove_child(tv->ctx, tv->tabs_host, tv->tabs [slot].tab_node);
	}
	for (int p = 0; p < tv->count; p++) {
		int slot = tv->order [p];
		if (tv->tabs [slot].closed) continue;
		xent_append_child(tv->ctx, tv->tabs_host, tv->tabs [slot].tab_node);
	}
}

static void tv_drag_swap(FluxTabViewData *tv, int pos_a, int pos_b) {
	int tmp           = tv->order [pos_a];
	tv->order [pos_a] = tv->order [pos_b];
	tv->order [pos_b] = tmp;
	tv_rebuild_tab_order(tv);
}

/* Swap with neighbours while the pointer has crossed past half their width. */
static float tv_drag_reorder_step(FluxTabViewData *tv, FluxTabViewItem *it, float dx) {
	for (;;) {
		int dir      = dx > 0.0f ? +1 : -1;
		int pos      = tv_order_pos(tv, it->index);
		int neighbor = tv_open_from(tv, pos, dir);
		if (neighbor < 0) return dx;
		float nw = tv->tabs [neighbor].cur_w;
		if (fabsf(dx) <= nw * 0.5f) return dx;
		tv_drag_swap(tv, pos, tv_order_pos(tv, neighbor));
		/* The displaced neighbour shifts by the dragged tab's width; let it
		 * slide from its old spot (ReorderThemeTransition). */
		FluxTabViewItem *nb  = &tv->tabs [neighbor];
		nb->slide_x0         = (dx > 0.0f) ? it->disp_w : -it->disp_w;
		nb->slide_start      = GetTickCount();
		nb->sliding          = true;
		tv->drag_press_x    += ( float ) ((dx > 0.0f) ? nw : -nw);
		dx                  -= ( float ) ((dx > 0.0f) ? nw : -nw);
	}
}

static void tv_drag_end(FluxTabViewData *tv) {
	if (tv->drag_slot >= 0) {
		FluxNodeData *nd = flux_node_store_get(tv->store, tv->tabs [tv->drag_slot].tab_node);
		if (nd) nd->render_translate_x = 0.0f;
	}
	tv->drag_slot   = -1;
	tv->drag_active = false;
	tv_repaint(tv);
}

static void tv_item_move(void *ctx, float x, float y) {
	( void ) y;
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	FluxTabViewData *tv = it->tv;
	if (tv->drag_slot != it->index) return;

	float dx = x - tv->drag_press_x;
	if (!tv->drag_active) {
		if (fabsf(dx) < ( float ) GetSystemMetrics(SM_CXDRAG)) return;
		tv->drag_active = true;
	}
	dx               = tv_drag_reorder_step(tv, it, dx);
	FluxNodeData *nd = flux_node_store_get(tv->store, it->tab_node);
	if (nd) nd->render_translate_x = dx;
	tv_repaint(tv);
}

static void tv_item_down(void *ctx, float x, float y, int clicks) {
	( void ) y;
	( void ) clicks;
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	FluxTabViewData *tv = it->tv;
	if (it->kind == FLUX_TAB_KIND_TAB && tv->can_reorder) {
		tv->drag_slot    = it->index;
		tv->drag_active  = false;
		tv->drag_press_x = x;
	}
}

static void tv_item_click(void *ctx) {
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	FluxTabViewData *tv = it->tv;
	if (it->kind == FLUX_TAB_KIND_ADD) {
		if (tv->on_add_tab) tv->on_add_tab(tv->cb_ctx);
		return;
	}
	bool dragged = tv->drag_active;
	tv_drag_end(tv);
	if (!dragged) tv_select(tv, it->index);
}

static void tv_item_cancel(void *ctx) {
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	tv_drag_end(it->tv);
}

/* WinUI: middle-button release closes a closable tab (TabViewItem::OnPointerReleased). */
static void tv_item_middle_click(void *ctx) {
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	if (it->kind == FLUX_TAB_KIND_TAB) tv_begin_close(it->tv, it->index);
}

static bool tv_ctrl_down(void) { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }

/* Arrows move focus between headers (not selection), Home/End jump the strip. */
static bool tv_key_focus(FluxTabViewData *tv, int from_slot, int vk) {
	int target = -1;
	if (vk == VK_LEFT) target = tv_open_from(tv, tv_order_pos(tv, from_slot), -1);
	if (vk == VK_RIGHT) target = tv_open_from(tv, tv_order_pos(tv, from_slot), +1);
	if (vk == VK_HOME) target = tv_open_from(tv, -1, +1);
	if (vk == VK_END) target = tv_open_from(tv, tv->count, -1);
	if (target >= 0 && tv->input) flux_input_set_focus(tv->input, tv->tabs [target].tab_node);
	return true;
}

static bool tv_key_ctrl_tab(FluxTabViewData *tv) {
	int dir = (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : +1;
	int n   = tv_open_from(tv, tv_order_pos(tv, tv->selected), dir);
	if (n < 0) n = tv_open_from(tv, dir > 0 ? -1 : tv->count, dir);
	if (n >= 0) tv_select(tv, n);
	return true;
}

static bool tv_key_ctrl_f4(FluxTabViewData *tv) {
	if (!tv_ctrl_down()) return false;
	tv_begin_close(tv, tv->selected);
	return true;
}

static bool tv_item_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	FluxTabViewData *tv = it->tv;
	switch (vk) {
	case VK_LEFT :
	case VK_RIGHT :
	case VK_HOME :
	case VK_END   : return tv_key_focus(tv, it->index, ( int ) vk);
	case VK_TAB   : return tv_ctrl_down() ? tv_key_ctrl_tab(tv) : false;
	case VK_F4    : return tv_key_ctrl_f4(tv);
	default       : return false;
	}
}

static bool tv_tick_close_anims(FluxTabViewData *tv, DWORD now) {
	bool active = false;
	for (int i = 0; i < tv->count; i++) {
		FluxTabViewItem *it = &tv->tabs [i];
		if (!it->closing) continue;
		float t = ( float ) (now - it->close_start) / TAB_CLOSE_MS;
		if (t >= 1.0f) {
			tv_finalize_close(tv, i);
			continue;
		}
		it->disp_w = it->close_w0 * (1.0f - flux_ease_out_cubic(t));
		it->cur_w  = it->disp_w;
		xent_set_size(tv->ctx, it->tab_node, (XentSize) {it->disp_w, FLUX_TAB_MIN_H});
		active = true;
	}
	return active;
}

static bool tv_tick_width_anims(FluxTabViewData *tv, DWORD now) {
	bool active = false;
	for (int i = 0; i < tv->count; i++) {
		FluxTabViewItem *it = &tv->tabs [i];
		if (!tv_slot_open(tv, i) || it->closing || !it->w_anim) continue;
		float t = ( float ) (now - it->w_start) / TAB_WIDTH_ANIM_MS;
		if (t >= 1.0f) {
			it->disp_w = it->cur_w;
			it->w_anim = false;
		}
		else {
			it->disp_w = it->w_from + (it->cur_w - it->w_from) * flux_ease_out_cubic(t);
			active     = true;
		}
		xent_set_size(tv->ctx, it->tab_node, (XentSize) {it->disp_w, FLUX_TAB_MIN_H});
	}
	return active;
}

static bool tv_tick_slides(FluxTabViewData *tv, DWORD now) {
	bool active = false;
	for (int i = 0; i < tv->count; i++) {
		FluxTabViewItem *it = &tv->tabs [i];
		if (!tv_slot_open(tv, i) || !it->sliding) continue;
		FluxNodeData *nd = flux_node_store_get(tv->store, it->tab_node);
		float         t  = ( float ) (now - it->slide_start) / TAB_SLIDE_MS;
		if (t >= 1.0f || tv->drag_slot == i) it->sliding = false;
		if (nd && tv->drag_slot != i)
			nd->render_translate_x = it->sliding ? it->slide_x0 * (1.0f - flux_ease_out_cubic(t)) : 0.0f;
		active |= it->sliding;
	}
	return active;
}

static bool tv_tick_scroll_anim(FluxTabViewData *tv, DWORD now) {
	if (!tv->scroll_anim) return false;
	FluxScrollData *sd = tv_scroll(tv);
	if (!sd) {
		tv->scroll_anim = false;
		return false;
	}
	float t = ( float ) (now - tv->scroll_start) / TAB_SCROLL_ANIM_MS;
	if (t >= 1.0f) {
		sd->scroll_x    = tv->scroll_target;
		tv->scroll_anim = false;
		return false;
	}
	sd->scroll_x = tv->scroll_from + (tv->scroll_target - tv->scroll_from) * flux_ease_out_cubic(t);
	return true;
}

static bool tv_tick_scroll_repeat(FluxTabViewData *tv, DWORD now) {
	if (tv->scroll_held == 0) return false;
	if (now >= tv->scroll_next) {
		tv_scroll_by(tv, ( float ) tv->scroll_held * FLUX_TAB_SCROLL_AMOUNT);
		tv->scroll_next = now + TAB_REPEAT_INTERVAL;
	}
	return true;
}

static bool tv_tick_pending_widths(FluxTabViewData *tv) {
	if (!tv->widths_valid) {
		float base = tv_avail_base(tv);
		if (base > 0.0f && fabsf(base - tv->settle_w) < 0.5f) {
			tv->widths_valid = true;
			tv_update_widths(tv, false);
		}
		tv->settle_w = base;
	}
	if (!tv->width_update_pending) return !tv->widths_valid;
	if (tv_strip_hovered(tv)) return true;
	tv_update_widths(tv, true);
	tv->width_update_pending = false;
	return false;
}

static bool tv_tick_one(FluxTabViewData *tv, DWORD now) {
	bool active  = tv_tick_close_anims(tv, now);
	active      |= tv_tick_width_anims(tv, now);
	active      |= tv_tick_slides(tv, now);
	active      |= tv_tick_scroll_anim(tv, now);
	active      |= tv_tick_scroll_repeat(tv, now);
	active      |= tv_tick_pending_widths(tv);
	return active;
}

static bool tv_step(void *ctx, unsigned long now) {
	FluxTabViewData *tv     = ( FluxTabViewData * ) ctx;
	bool             active = tv_tick_one(tv, ( DWORD ) now);
	tv_repaint(tv);
	if (!active) tv->anim_active = false;
	return active;
}

static void tv_on_window_resize(void *ctx, int width_px, int height_px) {
	( void ) width_px;
	( void ) height_px;
	FluxTabViewData *tv = ( FluxTabViewData * ) ctx;
	tv_update_widths(tv, false); /* resize tracks live geometry; no tween */
	tv_anim_start(tv);           /* re-measure once the post-resize layout lands */
}

static void tv_destroy(void *component_data) {
	FluxTabViewData *tv = ( FluxTabViewData * ) component_data;
	if (!tv) return;
	tv_anim_remove(tv);
	flux_window_remove_resize_observer(tv->window, tv_on_window_resize, tv);
	for (int i = 0; i < tv->count; i++) {
		flux_str_free(tv->tabs [i].label);
		flux_str_free(tv->tabs [i].icon_name);
	}
	free(tv);
}

static void tv_make_strip_item(FluxTabViewData *tv, FluxTabViewItem *slot, XentNodeId node, FluxTabKind kind) {
	*slot            = (FluxTabViewItem) {.tv = tv, .tab_node = node, .kind = kind, .index = -1};
	FluxNodeData *nd = flux_node_store_get(tv->store, node);
	if (!nd) return;
	nd->component_data        = slot;
	nd->behavior.on_click     = tv_item_click;
	nd->behavior.on_click_ctx = slot;
}

static void tv_make_scroll_button(FluxTabViewData *tv, XentNodeId *out, FluxTabViewItem *slot, FluxTabKind kind) {
	*out = flux_factory_create_node(tv->ctx, tv->store, tv->strip, FLUX_CONTROL_TAB_VIEW_ITEM);
	xent_set_size(tv->ctx, *out, (XentSize) {0.0f, 0.0f}); /* hidden until overflow */
	tv_make_strip_item(tv, slot, *out, kind);
	FluxNodeData *nd = flux_node_store_get(tv->store, *out);
	if (nd) {
		nd->behavior.on_pointer_down     = tv_scroll_btn_down;
		nd->behavior.on_pointer_down_ctx = slot;
		nd->behavior.on_click            = tv_scroll_btn_release; /* repeat ends on release */
		nd->behavior.on_click_ctx        = slot;
		nd->behavior.on_cancel           = tv_scroll_btn_release;
		nd->behavior.on_cancel_ctx       = slot;
	}
}

static void tv_make_tabs_host(FluxTabViewData *tv) {
	tv->tabs_host = flux_factory_create_node(tv->ctx, tv->store, tv->strip, FLUX_CONTROL_SCROLL);
	xent_set_protocol(tv->ctx, tv->tabs_host, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(tv->ctx, tv->tabs_host, XENT_FLEX_ROW);
	xent_set_flex_align_items(tv->ctx, tv->tabs_host, XENT_FLEX_ALIGN_END);
	xent_set_flex_grow(tv->ctx, tv->tabs_host, 1.0f);
	xent_set_size(tv->ctx, tv->tabs_host, (XentSize) {NAN, FLUX_TAB_MIN_H});

	FluxNodeData   *nd = flux_node_store_get(tv->store, tv->tabs_host);
	FluxScrollData *sd = ( FluxScrollData * ) calloc(1, sizeof(*sd));
	if (!nd || !sd) {
		free(sd);
		return;
	}
	sd->h_vis                  = FLUX_SCROLL_NEVER; /* WinUI hides the strip ScrollViewer bars */
	sd->v_vis                  = FLUX_SCROLL_NEVER;
	sd->content_h              = FLUX_TAB_MIN_H;
	sd->content_manual         = 1; /* strip extent tracks tab metrics, not child layout */
	nd->component_type         = FLUX_CONTROL_SCROLL;
	nd->component_data         = sd;
	nd->destroy_component_data = free;
}

static void tv_make_add_button(FluxTabViewData *tv) {
	tv->add_node = flux_factory_create_node(tv->ctx, tv->store, tv->strip, FLUX_CONTROL_TAB_VIEW_ITEM);
	xent_set_size(tv->ctx, tv->add_node, (XentSize) {FLUX_TAB_ADD_W, FLUX_TAB_ADD_H});
	xent_set_margin(tv->ctx, tv->add_node, (XentInsets) {FLUX_TAB_ADD_PAD_L, 0.0f, 0.0f, FLUX_TAB_ADD_PAD_B});
	tv_make_strip_item(tv, &tv->add_item, tv->add_node, FLUX_TAB_KIND_ADD);
	xent_set_focusable(tv->ctx, tv->add_node, true);
}

static void tv_build_tree(FluxTabViewData *tv, XentNodeId root) {
	xent_set_protocol(tv->ctx, root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(tv->ctx, root, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(tv->ctx, root, XENT_FLEX_ALIGN_STRETCH);

	tv->strip = flux_factory_create_node(tv->ctx, tv->store, root, FLUX_CONTROL_CONTAINER);
	xent_set_protocol(tv->ctx, tv->strip, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(tv->ctx, tv->strip, XENT_FLEX_ROW);
	xent_set_flex_align_items(tv->ctx, tv->strip, XENT_FLEX_ALIGN_END);
	xent_set_size(tv->ctx, tv->strip, (XentSize) {NAN, FLUX_TAB_STRIP_H});
	xent_set_padding(tv->ctx, tv->strip, (XentInsets) {0.0f, FLUX_TAB_STRIP_TOP_PAD, 0.0f, 0.0f});

	tv_make_scroll_button(tv, &tv->scroll_dec, &tv->scroll_items [0], FLUX_TAB_KIND_SCROLL_DEC);
	tv_make_tabs_host(tv);
	tv_make_scroll_button(tv, &tv->scroll_inc, &tv->scroll_items [1], FLUX_TAB_KIND_SCROLL_INC);
	if (tv->on_add_tab) tv_make_add_button(tv);

	tv->content = flux_factory_create_node(tv->ctx, tv->store, root, FLUX_CONTROL_CONTAINER);
	xent_set_flex_grow(tv->ctx, tv->content, 1.0f);
	xent_set_protocol(tv->ctx, tv->content, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(tv->ctx, tv->content, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(tv->ctx, tv->content, XENT_FLEX_ALIGN_STRETCH);
}

XentNodeId flux_create_tab_view(FluxTabViewCreateInfo const *info) {
	if (!info || !info->ctx || !info->store || !info->window) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_TAB_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData    *nd = flux_node_store_get(info->store, root);
	FluxTabViewData *tv = nd ? ( FluxTabViewData * ) calloc(1, sizeof(*tv)) : NULL;
	if (!nd || !tv) {
		free(tv);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}
	tv->ctx                    = info->ctx;
	tv->store                  = info->store;
	tv->window                 = info->window;
	tv->text                   = info->text;
	tv->input                  = info->input;
	tv->root                   = root;
	tv->selected               = -1;
	tv->drag_slot              = -1;
	tv->width_mode             = ( FluxTabWidthMode ) info->width_mode;
	tv->close_overlay          = ( FluxTabCloseOverlayMode ) info->close_overlay;
	tv->can_reorder            = !info->disable_reorder;
	tv->settle_w               = -1.0f;
	tv->strip_header           = XENT_NODE_INVALID;
	tv->strip_footer           = XENT_NODE_INVALID;
	tv->on_selection_changed   = info->on_selection_changed;
	tv->on_close_requested     = info->on_close_requested;
	tv->on_tab_closed          = info->on_tab_closed;
	tv->on_add_tab             = info->on_add_tab;
	tv->cb_ctx                 = info->userdata;

	nd->component_data         = tv;
	nd->destroy_component_data = tv_destroy;

	tv_build_tree(tv, root);
	flux_window_add_resize_observer(info->window, tv_on_window_resize, tv);
	return root;
}

static void tv_close_click(void *ctx) {
	FluxTabViewItem *it = ( FluxTabViewItem * ) ctx;
	tv_begin_close(it->tv, it->index);
}

/* The template CloseButton: a real child node so hover/press/click come from
 * the normal input pipeline (IsTabStop=False, hidden until selected/hover). */
static void tv_make_close_button(FluxTabViewData *tv, FluxTabViewItem *slot) {
	slot->close_node = flux_factory_create_node(tv->ctx, tv->store, slot->tab_node, FLUX_CONTROL_TAB_VIEW_ITEM);
	xent_set_size(tv->ctx, slot->close_node, (XentSize) {0.0f, 0.0f});
	FluxNodeData    *cn = flux_node_store_get(tv->store, slot->close_node);
	FluxTabViewItem *ci = &tv->close_items [slot->index];
	if (!cn) return;
	*ci = (FluxTabViewItem) {
	  .tv         = tv,
	  .tab_node   = slot->close_node,
	  .close_node = XENT_NODE_INVALID,
	  .kind       = FLUX_TAB_KIND_CLOSE,
	  .index      = slot->index};
	cn->component_data                = ci;
	cn->behavior.on_click             = tv_close_click;
	cn->behavior.on_click_ctx         = ci;
	cn->behavior.on_hover_changed     = tv_hover_changed;
	cn->behavior.on_hover_changed_ctx = slot;
}

static void tv_bind_tab_node(FluxTabViewData *tv, FluxTabViewItem *slot, XentNodeId tab, char const *label) {
	FluxNodeData *tn = flux_node_store_get(tv->store, tab);
	if (tn) {
		tn->component_data                = slot;
		tn->behavior.on_click             = tv_item_click;
		tn->behavior.on_click_ctx         = slot;
		tn->behavior.on_pointer_down      = tv_item_down;
		tn->behavior.on_pointer_down_ctx  = slot;
		tn->behavior.on_pointer_move      = tv_item_move;
		tn->behavior.on_pointer_move_ctx  = slot;
		tn->behavior.on_cancel            = tv_item_cancel;
		tn->behavior.on_cancel_ctx        = slot;
		tn->behavior.on_middle_click      = tv_item_middle_click;
		tn->behavior.on_middle_click_ctx  = slot;
		tn->behavior.on_hover_changed     = tv_hover_changed;
		tn->behavior.on_hover_changed_ctx = slot;
		tn->behavior.on_key               = tv_item_key;
		tn->behavior.on_key_ctx           = slot;
	}
	xent_set_focusable(tv->ctx, tab, true);
	xent_set_semantic_role(tv->ctx, tab, XENT_SEMANTIC_BUTTON);
	if (label) xent_set_semantic_label(tv->ctx, tab, label);
}

int flux_tab_view_add_tab(
  FluxNodeStore *store, XentNodeId tabview, char const *icon_name, char const *label, XentNodeId *out_content
) {
	FluxTabViewData *tv = tv_data(store, tabview);
	if (!tv || tv->count >= FLUX_TAB_VIEW_MAX_TABS) return -1;

	XentNodeId tab = flux_factory_create_node(tv->ctx, tv->store, tv->tabs_host, FLUX_CONTROL_TAB_VIEW_ITEM);
	if (tab == XENT_NODE_INVALID) return -1;

	XentNodeId content = flux_factory_create_node(tv->ctx, tv->store, XENT_NODE_INVALID, FLUX_CONTROL_CONTAINER);
	xent_set_flex_grow(tv->ctx, content, 1.0f); /* fills the content host when attached */

	int              idx  = tv->count++;
	FluxTabViewItem *slot = &tv->tabs [idx];
	*slot                 = (FluxTabViewItem) {
	  .tv        = tv,
	  .tab_node  = tab,
	  .content   = content,
	  .label     = flux_str_dup(label),
	  .icon_name = flux_str_dup(icon_name),
	  .kind      = FLUX_TAB_KIND_TAB,
	  .index     = idx,
	  .closable  = true};
	slot->close_node = XENT_NODE_INVALID;
	slot->width      = tv_measure_width(tv, icon_name, label, true);
	slot->cur_w      = slot->width;
	/* Runtime adds grow in from zero; pre-layout (startup) tabs snap in
	 * tv_animate_width instead. */
	slot->disp_w     = tv->widths_valid ? 0.0f : slot->width;
	tv->order [idx]  = idx;
	xent_set_size(tv->ctx, tab, (XentSize) {slot->disp_w, FLUX_TAB_MIN_H});

	/* Tabs keep their computed width and overflow into the scrollable host;
	 * without this the host's flex pass would squeeze them below MinWidth. */
	xent_set_flex_shrink(tv->ctx, tab, 0.0f);
	/* The tab lays out its close child itself: row, right-aligned, centered,
	 * 4px end padding (TabViewItemHeaderPadding right) -- correct for any width. */
	xent_set_protocol(tv->ctx, tab, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(tv->ctx, tab, XENT_FLEX_ROW);
	xent_set_flex_justify_content(tv->ctx, tab, XENT_FLEX_JUSTIFY_END);
	xent_set_flex_align_items(tv->ctx, tab, XENT_FLEX_ALIGN_CENTER);
	xent_set_padding(tv->ctx, tab, (XentInsets) {0.0f, 0.0f, FLUX_TAB_PAD_R, 0.0f});

	tv_bind_tab_node(tv, slot, tab, label);
	tv_make_close_button(tv, slot);
	tv_sync_close(tv, slot);
	tv_update_widths(tv, true);
	tv_anim_start(tv); /* re-measure against the real strip rect next frame */

	if (out_content) *out_content = content;
	if (tv->selected < 0) tv_select(tv, idx);
	return idx;
}

void flux_tab_view_select(FluxNodeStore *store, XentNodeId tabview, int index) {
	FluxTabViewData *tv = tv_data(store, tabview);
	if (tv) tv_select(tv, index);
}

void flux_tab_view_close(FluxNodeStore *store, XentNodeId tabview, int index) {
	FluxTabViewData *tv = tv_data(store, tabview);
	if (tv) tv_begin_close(tv, index);
}

void flux_tab_view_set_closable(FluxNodeStore *store, XentNodeId tabview, int index, bool closable) {
	FluxTabViewData *tv = tv_data(store, tabview);
	if (!tv || index < 0 || index >= tv->count) return;
	tv->tabs [index].closable = closable;
	tv->tabs [index].width    = tv_measure_width(tv, tv->tabs [index].icon_name, tv->tabs [index].label, closable);
	tv_sync_close(tv, &tv->tabs [index]);
	tv_update_widths(tv, true);
	tv_repaint(tv);
}

void flux_tab_view_set_width_mode(FluxNodeStore *store, XentNodeId tabview, int width_mode) {
	FluxTabViewData *tv = tv_data(store, tabview);
	if (!tv) return;
	tv->width_mode = ( FluxTabWidthMode ) width_mode;
	tv_update_widths(tv, true);
	tv_repaint(tv);
}

/* Rebuild the strip child order: [header][dec][tabs][inc][add][footer]. */
static void tv_restack_strip(FluxTabViewData *tv) {
	XentNodeId seq [6]
	  = {tv->strip_header, tv->scroll_dec, tv->tabs_host, tv->scroll_inc, tv->add_node, tv->strip_footer};
	for (int i = 0; i < 6; i++)
		if (seq [i] != XENT_NODE_INVALID) xent_remove_child(tv->ctx, tv->strip, seq [i]);
	for (int i = 0; i < 6; i++)
		if (seq [i] != XENT_NODE_INVALID) xent_append_child(tv->ctx, tv->strip, seq [i]);
}

void flux_tab_view_set_strip_header(FluxNodeStore *store, XentNodeId tabview, XentNodeId node) {
	FluxTabViewData *tv = tv_data(store, tabview);
	if (!tv) return;
	tv->strip_header = node;
	xent_append_child(tv->ctx, tv->strip, node);
	tv_restack_strip(tv);
}

void flux_tab_view_set_strip_footer(FluxNodeStore *store, XentNodeId tabview, XentNodeId node) {
	FluxTabViewData *tv = tv_data(store, tabview);
	if (!tv) return;
	tv->strip_footer = node;
	xent_append_child(tv->ctx, tv->strip, node);
	tv_restack_strip(tv);
}
