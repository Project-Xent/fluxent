#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_ids.h"
#include "a11y/flux_uia_object.h"
#include "bridge/flux_internal.h"
#include "controls/flux_module.h"
#include "controls/flux_control_uia.h"
#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "controls/draw/flux_control_draw.h"
#include "render/flux_anim.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/behavior/flux_nav_internal.h"
#include "controls/behavior/flux_uia_part.h"
#include "fluxent/flux_window.h"
#include "fluxent/flux_popup.h"
#include "controls/data/flux_title_bar_data.h"
#include "input/flux_input_internal.h"
#include "host/flux_prop_apply.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>
#include <xtk/xtk_navmodel.h>

#define NAV_BACK_WIDTH       40.0f
#define NAV_BACK_HEIGHT      36.0f
#define NAV_BACK_ROW_HEIGHT  40.0f
#define NAV_BACK_MARGIN_TOP  6.0f
#define NAV_TOGGLE_TOP       6.0f
#define NAV_TOGGLE_PAD_X     4.0f
#define NAV_TITLE_X          44.0f
#define NAV_PANE_PAD_X       4.0f
#define NAV_MIN_HEADER_X     -24.0f
#define NAV_MIN_HEADER_Y     44.0f

#define NAV_PANE_ANIM_MS     250.0f
#define NAV_OVERLAY_OPEN_MS  350.0f
#define NAV_OVERLAY_CLOSE_MS 120.0f
#define NAV_IND_MS           600.0f
#define NAV_CONTENT_MS       250.0f
#define NAV_CONTENT_DY       20.0f
#define NAV_ADAPT_COMPACT    641.0f
#define NAV_ADAPT_EXPANDED   1008.0f
#define NAV_EXPAND_MS        200.0f
#define NAV_CHEVRON_ZONE     40.0f

static float nav_ease_lead(float t) {
	if (t >= 1.0f / 3.0f) return 1.0f;
	return flux_cubic_bezier(t * 3.0f, 0.9f, 0.1f, 1.0f, 0.2f);
}

static void nav_tween_init(FluxNavTween *tw, float v) {
	tw->current = tw->start = tw->target = v;
	tw->active                           = false;
}

static void nav_tween_to(FluxNavTween *tw, float target, float duration_ms) {
	if (fabsf(target - tw->target) < 0.01f) return;
	tw->start    = tw->current;
	tw->target   = target;
	tw->start_ms = flux_anim_now();
	tw->duration = duration_ms;
	tw->active   = true;
}

void flux_nav_tween_snap(FluxNavTween *tw, float v) {
	tw->current = tw->start = tw->target = v;
	tw->active                           = false;
}

static bool nav_tween_tick(FluxNavTween *tw, DWORD now, float (*ease)(float)) {
	if (!tw->active) return false;
	float t = tw->duration > 0.0f ? ( float ) (now - tw->start_ms) / tw->duration : 1.0f;
	if (t >= 1.0f) {
		tw->current = tw->target;
		tw->active  = false;
		return false;
	}
	tw->current = tw->start + (tw->target - tw->start) * ease(t < 0.0f ? 0.0f : t);
	return true;
}

static FluxNavViewData *nav_data(FluxStore *store, XentNodeId nav) {
	FluxNodeData *nd = flux_store_find(store, nav);
	if (!nd || nd->component_type != FLUX_CONTROL_NAV_VIEW) return NULL;
	return ( FluxNavViewData * ) nd->component_data;
}

FluxNavViewData *flux_nav_data(FluxStore *store, XentNodeId nav) { return nav_data(store, nav); }

void             flux_nav_repaint(FluxNavViewData *d) {
	HWND h = flux_window_hwnd(d->window);
	if (h) InvalidateRect(h, NULL, FALSE);
	if (d->fly_owner != FLUX_NAV_FLY_NONE) InvalidateRect(flux_popup_hwnd(d->fly_popup), NULL, FALSE);
}

static float nav_item_height(FluxNavItemKind kind);
static void  nav_toggle_expand(FluxNavViewData *d, int idx);
static void  nav_apply_geometry(FluxNavViewData *d);
static void  nav_indicator_reset(FluxNavViewData *d);
static void  nav_set_pane_padding(FluxNavViewData *d);

bool         flux_nav_pane_overlay(FluxNavViewData const *d) {
	return d->mode == FLUX_NAV_COMPACT || d->mode == FLUX_NAV_MINIMAL;
}

bool flux_nav_closed_compact(FluxNavViewData const *d) {
	return !d->pane_open && (d->mode == FLUX_NAV_EXPANDED || d->mode == FLUX_NAV_COMPACT);
}

bool flux_nav_flyout_repeater(FluxNavViewData const *d, FluxNavViewItem const *it) {
	if (!it->has_children) return false;
	return (flux_nav_closed_compact(d) && it->parent < 0) || it->place == FLUX_NAV_PLACE_TOP;
}

uint8_t flux_nav_draw_depth(FluxNavViewData const *d, FluxNavViewItem const *it) {
	for (int p = it->parent; p >= 0; p = d->items [p].parent)
		if (flux_nav_flyout_repeater(d, &d->items [p])) return ( uint8_t ) (it->depth - d->items [p].depth - 1);
	return ( uint8_t ) it->depth;
}

bool flux_nav_shows_label(FluxNavViewData const *d, FluxNavViewItem const *it) {
	if (!it->label) return false;
	if (it->kind == FLUX_NAV_ITEM_SETTINGS && it->place != FLUX_NAV_PLACE_LEFT) return false;
	return it->place != FLUX_NAV_PLACE_LEFT || it->parent >= 0 || d->labels_visible;
}

float flux_nav_text_w(FluxNavViewData const *d, char const *text, uint16_t weight) {
	if (!text || !text [0]) return 0.0f;
	XentTextMetrics    tm  = {0};
	XentTextMeasureReq req = {
	  .text              = text,
	  .font_size         = FLUX_NAV_LABEL_FONT,
	  .font_weight       = weight,
	  .line_break_policy = XENT_LINEBREAK_NOWRAP,
	  .width_mode        = XENT_MEASURE_UNDEFINED};
	return xent_text_measure(d->ctx, &req, &tm) ? ceilf(tm.width) : 0.0f;
}

static bool nav_host_inline(FluxNavViewData const *d, FluxNavViewItem const *it) {
	if (!it->has_children || flux_nav_flyout_repeater(d, it)) return false;
	return it->expanded || it->expand_t.active || it->expand_t.current > 0.001f;
}

float flux_nav_host_full_h(FluxNavViewData const *d, int parent) {
	float h = 0.0f;
	int   n = 0;
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem const *c = &d->items [i];
		if (c->parent != parent) continue;
		h += FLUX_NAV_ITEM_HEIGHT;
		n++;
		if (!nav_host_inline(d, c)) continue;
		h += c->expand_t.current * flux_nav_host_full_h(d, i);
		n++;
	}
	float gap = d->mode == FLUX_NAV_TOP ? 0.0f : FLUX_NAV_ITEM_GAP;
	return n > 0 ? h + gap * ( float ) (n - 1) : 0.0f;
}

static void nav_sync_host(FluxNavViewData *d, int i) {
	FluxNavViewItem *it     = &d->items [i];
	bool             in_fly = d->fly_owner == i;
	bool             shown  = in_fly || nav_host_inline(d, it);
	xent_setcollapsed(d->ctx, it->children_host, !shown);
	xent_setgap(d->ctx, it->children_host, d->mode == FLUX_NAV_TOP ? 0.0f : FLUX_NAV_ITEM_GAP);
	float h = in_fly ? flux_nav_host_full_h(d, i) : it->expand_t.current * flux_nav_host_full_h(d, i);
	xent_setsize(d->ctx, it->children_host, (XentSize) {NAN, shown ? h : 0.0f});
}

void flux_nav_sync_hosts(FluxNavViewData *d) {
	for (int i = d->count - 1; i >= 0; i--)
		if (d->items [i].has_children) nav_sync_host(d, i);
}

void flux_nav_place(FluxNavViewData *d, XentNodeId parent, XentNodeId node, XentNodeId before) {
	if (xent_node_parent(d->ctx, node) == parent) xent_node_move(d->ctx, parent, node, before);
	else xent_node_reparent(d->ctx, parent, node, before);
}

void flux_nav_graft_host(FluxNavViewData *d, FluxNavViewItem *it, bool on) {
	FluxNodeData *hn = flux_store_find(d->store, it->children_host);
	FluxNodeData *pn = flux_store_find(d->store, it->node);
	if (!hn || !pn) return;
	hn->uia_adopter = on ? it->node : XENT_NODE_INVALID;
	pn->uia_graft   = on ? it->children_host : XENT_NODE_INVALID;
}

void flux_nav_home_host(FluxNavViewData *d, FluxNavViewItem *it) {
	if (it->children_host == XENT_NODE_INVALID) return;
	flux_nav_graft_host(d, it, true);
	XentNodeId container = xent_node_parent(d->ctx, it->node);
	XentNodeId next      = xent_node_next(d->ctx, it->node);
	if (next == it->children_host) return;
	flux_nav_place(d, container, it->children_host, next);
}

static void nav_set_abs(FluxNavViewData *d, XentNodeId node, float x, float y, float w, float h) {
	xent_setpos(d->ctx, node, (XentPoint) {x, y});
	xent_setsize(d->ctx, node, (XentSize) {w, h});
}

static void nav_sync_header_heights(FluxNavViewData *d) {
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (it->kind != FLUX_NAV_ITEM_HEADER || it->place == FLUX_NAV_PLACE_TOP) continue;
		bool shown = it->place != FLUX_NAV_PLACE_LEFT || d->labels_visible;
		xent_setsize(d->ctx, it->node, (XentSize) {NAN, shown ? FLUX_NAV_HEADER_H : 0.0f});
	}
}

static void nav_place_pane(FluxNavViewData *d, XentRect clip, float content_width) {
	nav_set_abs(d, d->pane_clip, clip.x, clip.y, clip.w, clip.h);
	nav_set_abs(d, d->pane, 0.0f, 0.0f, content_width, clip.h);
}

static float nav_titlebar_height(FluxNavViewData const *d) {
	if (!d->window_title_bar) return 0.0f;
	return d->tall_title_bar ? FLUX_TB_TALL_HEIGHT : FLUX_TB_HEIGHT;
}

static void nav_place_content(FluxNavViewData *d, float inset, float top) {
	top += d->content_overlay_h;
	nav_set_abs(d, d->content_grid, inset, top, fmaxf(0.0f, d->width - inset), fmaxf(0.0f, d->height - top));
}

static bool nav_back_visible(FluxNavViewData const *d) {
	return d->back_button != XTK_NAV_BACK_COLLAPSED && !(d->mode == FLUX_NAV_MINIMAL && d->pane_open);
}

static bool nav_close_visible(FluxNavViewData const *d) {
	return d->back_button != XTK_NAV_BACK_COLLAPSED
	    && d->mode == FLUX_NAV_MINIMAL
	    && d->pane_open
	    && (d->pane_mode == FLUX_NAV_PANE_AUTO || d->pane_mode == FLUX_NAV_PANE_LEFT_MINIMAL);
}

float flux_nav_overlay_button_width(FluxNavViewData const *d) {
	return d->mode == FLUX_NAV_MINIMAL && (nav_back_visible(d) || nav_close_visible(d)) ? NAV_BACK_WIDTH : 0.0f;
}

float        flux_nav_top_back_width(FluxNavViewData const *d) { return nav_back_visible(d) ? NAV_BACK_WIDTH : 0.0f; }

static float nav_back_row_height(FluxNavViewData const *d) {
	return d->mode != FLUX_NAV_MINIMAL && nav_back_visible(d) ? NAV_BACK_ROW_HEIGHT : 0.0f;
}

static void nav_layout_back_buttons(FluxNavViewData *d) {
	xent_setcollapsed(d->ctx, d->back, !nav_back_visible(d));
	xent_setcollapsed(d->ctx, d->close, !nav_close_visible(d));
	float y = nav_titlebar_height(d) + NAV_BACK_MARGIN_TOP;
	if (d->mode != FLUX_NAV_TOP) y += d->content_overlay_h;
	nav_set_abs(d, d->back, FLUX_NAV_ITEM_MARGIN_H, y, NAV_BACK_WIDTH, NAV_BACK_HEIGHT);
	nav_set_abs(d, d->close, FLUX_NAV_ITEM_MARGIN_H, y, NAV_BACK_WIDTH, NAV_BACK_HEIGHT);
}

static bool nav_content_slot_layout(FluxNavViewData *d, float width) {
	XentCtx *ctx     = d->ctx;
	bool     minimal = d->mode == FLUX_NAV_MINIMAL;
	bool     header  = (d->always_show_header || minimal) && xent_node_first(ctx, d->page_header) != XENT_NODE_INVALID;
	bool     changed = header == xent_collapsed(ctx, d->page_header);
	xent_setcollapsed(ctx, d->page_header, !header);
	float      left   = FLUX_NAV_TOGGLE_W + flux_nav_overlay_button_width(d) + NAV_MIN_HEADER_X;
	XentInsets margin = minimal ? (XentInsets) {left, NAV_MIN_HEADER_Y, 0, 0} : (XentInsets) {0};
	xent_setm(ctx, d->page_header, margin);
	XentNodeId overlay = xent_node_first(ctx, d->content_overlay);
	XentSize   size    = {0};
	if (overlay) xent_layout_desired_size(ctx, overlay, (XentSize) {width, INFINITY}, &size);
	changed              |= d->content_overlay_h != size.h;
	d->content_overlay_h  = size.h;
	return changed;
}

static void nav_place_content_overlay(FluxNavViewData *d) {
	float y = nav_titlebar_height(d) + (d->mode == FLUX_NAV_TOP ? FLUX_NAV_TOP_H : 0.0f);
	nav_set_abs(d, d->content_overlay, 0.0f, y, d->width, d->content_overlay_h);
}

static void nav_apply_geometry_top(FluxNavViewData *d) {
	d->labels_visible = true;
	float tb          = nav_titlebar_height(d);
	xent_setsize(d->ctx, d->toggle, (XentSize) {0.0f, 0.0f});
	if (d->title_bar != XENT_NODE_INVALID) nav_set_abs(d, d->title_bar, 0.0f, 0.0f, d->width, tb);
	nav_place_pane(d, (XentRect) {0.0f, tb, d->width, FLUX_NAV_TOP_H}, d->width);
	nav_place_content(d, 0.0f, tb + FLUX_NAV_TOP_H);
	nav_set_abs(d, d->scrim, 0.0f, 0.0f, 0.0f, 0.0f);
	flux_nav_top_arrange(d);
}

static float nav_open_pane_length(FluxNavViewData const *d) { return fminf(d->width, FLUX_NAV_PANE_EXPANDED); }

static void  nav_place_left_pane(FluxNavViewData *d, float x, float width, float flow) {
	float y = d->content_overlay_h;
	nav_place_pane(d, (XentRect) {x, y, width, fmaxf(0.0f, d->height - y)}, flow);
}

static float nav_compact_width(FluxNavViewData const *d) {
	return FLUX_NAV_PANE_COMPACT + d->pane_progress.current * (nav_open_pane_length(d) - FLUX_NAV_PANE_COMPACT);
}

static void nav_apply_geometry_overlay(FluxNavViewData *d) {
	bool  minimal = d->mode == FLUX_NAV_MINIMAL;
	float top     = nav_titlebar_height(d);
	if (minimal && !d->window_title_bar && xent_collapsed(d->ctx, d->page_header))
		top = FLUX_NAV_TOGGLE_H + FLUX_NAV_PANE_TOP_PAD;
	if (minimal) {
		float length = nav_open_pane_length(d);
		float x      = (d->pane_progress.current - 1.0f) * length;
		nav_place_left_pane(d, x, length, length);
		nav_place_content(d, 0.0f, top);
	}
	else {
		float content_width = d->pane_open ? nav_open_pane_length(d) : FLUX_NAV_PANE_COMPACT;
		nav_place_left_pane(d, 0.0f, nav_compact_width(d), content_width);
		nav_place_content(d, FLUX_NAV_PANE_COMPACT, top);
	}
	XentSize scrim = d->pane_open ? (XentSize) {d->width, d->height} : (XentSize) {0};
	nav_set_abs(d, d->scrim, 0.0f, 0.0f, scrim.w, scrim.h);
}

static void nav_apply_geometry_inline(FluxNavViewData *d) {
	float flow = nav_compact_width(d);
	float tb   = nav_titlebar_height(d);
	nav_place_left_pane(d, 0.0f, flow, flow);
	nav_place_content(d, flow, tb);
	nav_set_abs(d, d->scrim, 0.0f, 0.0f, 0.0f, 0.0f);
}

static void nav_set_tab_inert(FluxNavViewData const *d, XentNodeId node, bool inert) {
	FluxNodeData *nd = flux_store_find(d->store, node);
	if (nd) nd->tab_inert = inert;
}

static void nav_sync_pane_tab_scope(FluxNavViewData const *d) {
	bool          close = nav_close_visible(d);
	FluxNodeData *root  = flux_store_find(d->store, d->root);
	FluxNodeData *pane  = flux_store_find(d->store, d->pane_clip);
	if (root) root->tab_cycle = close;
	if (pane) pane->tab_cycle = !close && flux_nav_pane_overlay(d) && d->pane_open;
	nav_set_tab_inert(d, d->pane_clip, d->mode == FLUX_NAV_MINIMAL && !d->pane_open);
	nav_set_tab_inert(d, d->toggle, close);
	nav_set_tab_inert(d, d->title_bar, close);
	nav_set_tab_inert(d, d->content_grid, close);
	nav_set_tab_inert(d, d->content_overlay, close);
}

static void nav_layout_toggle_title(FluxNavViewData *d, float width) {
	float    w    = fmaxf(0.0f, width - NAV_TITLE_X);
	XentSize size = {0};
	xent_setsize(d->ctx, d->chrome_title, (XentSize) {w, NAN});
	xent_layout_desired_size(d->ctx, d->chrome_title, (XentSize) {w, INFINITY}, &size);
	nav_set_abs(d, d->chrome_title, NAV_TITLE_X, (FLUX_NAV_TOGGLE_H - size.h) * 0.5f - 1.0f, w, size.h);
}

static void nav_layout_toggle(FluxNavViewData *d) {
	if (d->window_title_bar) {
		xent_setsize(d->ctx, d->toggle, (XentSize) {0});
		return;
	}
	float width   = FLUX_NAV_TOGGLE_W;
	float leading = flux_nav_overlay_button_width(d);
	if (!xent_collapsed(d->ctx, d->chrome_title)) width = nav_open_pane_length(d) - 2.0f * NAV_TOGGLE_PAD_X - leading;
	float y = d->content_overlay_h + NAV_TOGGLE_TOP + nav_back_row_height(d);
	nav_set_abs(d, d->toggle, NAV_TOGGLE_PAD_X + leading, y, width, FLUX_NAV_TOGGLE_H);
	if (xent_node_parent(d->ctx, d->chrome_title) == d->toggle) nav_layout_toggle_title(d, width);
}

static void nav_apply_geometry(FluxNavViewData *d) {
	nav_sync_pane_tab_scope(d);
	nav_content_slot_layout(d, d->width);
	nav_layout_back_buttons(d);
	nav_set_pane_padding(d);
	nav_place_content_overlay(d);
	if (d->mode == FLUX_NAV_TOP) {
		nav_apply_geometry_top(d);
		return;
	}
	bool labels_was   = d->labels_visible;
	d->labels_visible = !flux_nav_closed_compact(d);
	if (d->labels_visible != labels_was) {
		nav_sync_header_heights(d);

		nav_indicator_reset(d);
	}

	if (d->title_bar != XENT_NODE_INVALID) nav_set_abs(d, d->title_bar, 0.0f, 0.0f, d->width, nav_titlebar_height(d));
	flux_nav_chrome_sync(d);
	nav_layout_toggle(d);

	if (flux_nav_pane_overlay(d)) {
		nav_apply_geometry_overlay(d);
		return;
	}

	nav_apply_geometry_inline(d);
}

static bool             nav_index_live(int index) { return index >= 0 || index == FLUX_NAV_SETTINGS; }

static FluxNavViewItem *nav_index_item(FluxNavViewData *d, int index) {
	if (index == FLUX_NAV_SETTINGS) return d->settings != XENT_NODE_INVALID ? &d->settings_item : NULL;
	if (index < 0 || index >= d->count) return NULL;
	return &d->items [index];
}

static int nav_indicator_climb(FluxNavViewData *d, int cur) {
	if (flux_nav_fly_shows(d, &d->items [cur])) return cur;
	for (int p = d->items [cur].parent; p >= 0; p = d->items [p].parent)
		if (flux_nav_flyout_repeater(d, &d->items [p]) || !d->items [p].expanded) cur = p;
	return d->items [cur].in_overflow ? -1 : cur;
}

static int nav_indicator_item(FluxNavViewData *d) {
	int cur = d->selected;
	if (d->mode == FLUX_NAV_MINIMAL && !d->pane_open) return -1;
	if (cur == FLUX_NAV_SETTINGS) return d->settings_visible ? cur : -1;
	if (cur < 0) return -1;
	return nav_indicator_climb(d, cur);
}

static XentNodeId nav_tab_entry(void *ctx, XentNodeId group) {
	FluxNavViewData *d    = ctx;
	int              item = nav_indicator_item(d);
	if (item == FLUX_NAV_SETTINGS) return input_within(d->ctx, d->settings, group) ? d->settings : XENT_NODE_INVALID;
	for (; item >= 0; item = d->items [item].parent) {
		XentNodeId node = d->items [item].node;
		if (input_within(d->ctx, node, group)) return node;
	}
	return XENT_NODE_INVALID;
}

static void nav_bind_tab_group(FluxNavViewData *d, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(d->store, node);
	if (!nd) return;
	nd->behavior.tab_entry     = nav_tab_entry;
	nd->behavior.tab_entry_ctx = d;
}

FluxRect flux_nav_indicator_rect(FluxRect row, uint8_t place, uint8_t depth, bool in_flyout) {
	if (place == FLUX_NAV_PLACE_TOP)
		return (FluxRect) {
		  row.x + (row.w - FLUX_NAV_TOP_IND_W) * 0.5f, row.y + row.h - 6.0f - FLUX_NAV_TOP_IND_H, FLUX_NAV_TOP_IND_W,
		  FLUX_NAV_TOP_IND_H};
	if (place == FLUX_NAV_PLACE_OVERFLOW) return (FluxRect) {row.x + 4.0f, row.y + (row.h - 24.0f) * 0.5f, 2.0f, 24.0f};
	float x = row.x + ( float ) depth * 31.0f - (in_flyout ? 0.0f : 1.0f);
	return (FluxRect) {x, row.y + (row.h - FLUX_NAV_INDICATOR_H) * 0.5f, FLUX_NAV_INDICATOR_W, FLUX_NAV_INDICATOR_H};
}

static FluxNavIndicator nav_indicator_at(FluxNavViewData *d, int index) {
	FluxNavViewItem const *it = nav_index_item(d, index);
	if (!it) return (FluxNavIndicator) {.item = -1};
	FluxRect row = flux_binding_window_rect(d->ctx, d->store, it->node);
	if (row.w <= 0.0f || row.h <= 0.0f) return (FluxNavIndicator) {.item = -1};

	bool     fly  = flux_nav_fly_shows(d, it);
	HWND     hwnd = fly ? flux_popup_hwnd(d->fly_popup) : flux_window_hwnd(d->window);
	FluxRect pill = flux_nav_indicator_rect(row, it->place, flux_nav_draw_depth(d, it), fly);
	if (xent_resolved_dir(d->ctx, it->node) == XENT_DIRECTION_RTL) pill.x = 2.0f * row.x + row.w - pill.x;
	POINT origin = {0};
	ClientToScreen(hwnd, &origin);
	float scale = ( float ) GetDpiForWindow(hwnd) / 96.0f;
	return (FluxNavIndicator) {
	  {origin.x + pill.x * scale, origin.y + pill.y * scale},
      index, it->place, fly
    };
}

static bool nav_same_indicator(FluxNavIndicator a, FluxNavIndicator b) {
	return a.item == b.item && a.place == b.place && a.in_flyout == b.in_flyout;
}

static bool nav_motion_enabled(void) {
	BOOL enabled = TRUE;
	SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
	return enabled;
}

static float nav_cross_ease(float t) { return flux_cubic_bezier(t, 0.41f, 0.52f, 0.0f, 0.94f); }

static float nav_indicator_extent(FluxNavIndicator indicator, bool horizontal) {
	FluxRect pill = flux_nav_indicator_rect((FluxRect) {0}, indicator.place, 0, indicator.in_flyout);
	return horizontal ? pill.w : pill.h;
}

static FluxNavIndicatorFrame nav_same_level_frame(FluxNavViewData const *d, bool outgoing, bool rtl, bool fly) {
	bool  horizontal  = d->mode == FLUX_NAV_TOP;
	float begin       = nav_indicator_extent(d->outgoing, horizontal);
	float end         = nav_indicator_extent(d->indicator, horizontal);
	float dimension   = outgoing ? begin : end;
	float begin_scale = begin / dimension;
	float end_scale   = end / dimension;
	float travel      = rtl && horizontal ? -d->ind_travel : d->ind_travel;
	float from        = outgoing ? 0.0f : -travel;
	float to          = from + travel;
	bool  forward     = travel > 0.0f;

	float t           = d->ind_progress.current;
	float stretch     = 0.333f;
	float peak        = fabsf(travel) / dimension + (forward ? end_scale : begin_scale);
	float fade        = flux_cubic_bezier(flux_maxf(0.0f, (t - stretch) / (1.0f - stretch)), 0.1f, 0.9f, 0.2f, 1.0f);
	float first       = flux_cubic_bezier(flux_minf(1.0f, t / stretch), 0.9f, 0.1f, 1.0f, 0.2f);
	float scale       = begin_scale + (peak - begin_scale) * first + (end_scale - peak) * fade;

	float start       = forward ? from : from + dimension * (begin_scale - 1.0f);
	float finish      = forward ? to + dimension * (end_scale - 1.0f) : to;
	float position    = t < stretch ? start : finish;
	float pivot       = (t < 1.0f / 3.0f) != forward ? dimension : 0.0f;
	return (FluxNavIndicatorFrame) {
	  .scale      = scale,
	  .offset     = position + pivot * (1.0f - scale),
	  .opacity    = outgoing ? 1.0f - fade : 1.0f,
	  .horizontal = horizontal,
	  .in_flyout  = fly};
}

static FluxNavIndicatorFrame nav_cross_level_frame(FluxNavViewData const *d, bool outgoing, bool fly) {
	FluxNavIndicator indicator  = outgoing ? d->outgoing : d->indicator;
	bool             horizontal = indicator.place == FLUX_NAV_PLACE_TOP;
	float            progress   = nav_cross_ease(d->ind_progress.current);
	float            scale      = outgoing ? 1.0f - progress : progress;
	bool             end_pivot  = !horizontal && outgoing == d->ind_down;
	float            pivot      = nav_indicator_extent(indicator, indicator.place == FLUX_NAV_PLACE_OVERFLOW);
	return (FluxNavIndicatorFrame) {
	  .scale      = scale,
	  .offset     = end_pivot ? pivot * (1.0f - scale) : 0.0f,
	  .opacity    = 1.0f,
	  .horizontal = horizontal,
	  .in_flyout  = fly};
}

FluxNavIndicatorFrame flux_nav_indicator_frame(FluxNavViewItem const *it, bool rtl) {
	FluxNavViewData const *d = it->nav;
	if (it->index < 0 && it->kind != FLUX_NAV_ITEM_SETTINGS) return (FluxNavIndicatorFrame) {0};
	bool             fly      = flux_nav_fly_shows(d, it);
	FluxNavIndicator item     = {.item = it->index, .place = it->place, .in_flyout = fly};
	bool             incoming = nav_same_indicator(item, d->indicator);
	if (!d->ind_progress.active) return (FluxNavIndicatorFrame) {.scale = incoming, .opacity = 1.0f, .in_flyout = fly};
	bool outgoing = nav_same_indicator(item, d->outgoing);
	if (!incoming && !outgoing) return (FluxNavIndicatorFrame) {0};
	if (d->ind_same_level) return nav_same_level_frame(d, outgoing, rtl, fly);
	return nav_cross_level_frame(d, outgoing, fly);
}

static void nav_indicator_reset(FluxNavViewData *d) {
	flux_nav_tween_snap(&d->ind_progress, 0.0f);
	d->indicator          = (FluxNavIndicator) {.item = -1};
	d->ind_layout_pending = false;
}

static void nav_indicator_transition(FluxNavViewData *d, FluxNavIndicator prev, FluxNavIndicator next) {
	flux_nav_tween_snap(&d->ind_progress, 0.0f);
	if (!nav_index_live(prev.item) || !nav_index_live(next.item) || !nav_motion_enabled()) return;
	float dx          = next.position.x - prev.position.x;
	float dy          = next.position.y - prev.position.y;
	bool  top         = d->mode == FLUX_NAV_TOP;
	HWND  hwnd        = next.in_flyout ? flux_popup_hwnd(d->fly_popup) : flux_window_hwnd(d->window);
	d->outgoing       = prev;
	d->ind_same_level = (top ? dy : dx) == 0.0f;
	d->ind_travel     = (top ? dx : dy) * 96.0f / GetDpiForWindow(hwnd);
	d->ind_down       = dy > 0.0f;
	nav_tween_to(&d->ind_progress, 1.0f, NAV_IND_MS);
}

static float            nav_linear(float t) { return t; }

static FluxNavIndicator nav_indicator_previous(FluxNavViewData *d) {
	FluxNavIndicator       prev = d->indicator;
	FluxNavViewItem const *it   = nav_index_item(d, prev.item);
	if (!it) return (FluxNavIndicator) {.item = -1};
	if (!prev.in_flyout && it->in_overflow) return (FluxNavIndicator) {.item = -1};
	FluxNavIndicator current = nav_indicator_at(d, prev.item);
	return nav_same_indicator(prev, current) ? current : prev;
}

static bool nav_indicator_tick(FluxNavViewData *d, DWORD now) {
	if (d->ind_layout_pending) {
		d->ind_layout_pending = false;
		return true;
	}

	nav_tween_tick(&d->ind_progress, now, nav_linear);
	int              index = nav_indicator_item(d);
	bool             live  = nav_index_live(index);
	FluxNavIndicator next  = live ? nav_indicator_at(d, index) : (FluxNavIndicator) {.item = -1};
	if (live && !nav_index_live(next.item)) return true;
	FluxNavIndicator prev = nav_indicator_previous(d);
	if (!nav_same_indicator(prev, next)) nav_indicator_transition(d, prev, next);
	d->indicator = next;
	return d->ind_progress.active;
}

static void nav_anim_remove(FluxNavViewData *d) {
	d->anim_active = false;
	flux_anim_unregister(d);
}

static bool nav_tick_expand(FluxNavViewData *d, DWORD now) {
	bool active = false;
	for (int i = 0; i < d->count; i++)
		if (d->items [i].has_children) active |= nav_tween_tick(&d->items [i].expand_t, now, nav_ease_lead);
	flux_nav_sync_hosts(d);
	return active;
}

static void nav_apply_content_motion(FluxNavViewData *d) {
	FluxNodeData *cn = flux_store_find(d->store, d->content);
	if (!cn) return;
	float p = d->content_t.current;
	if (p < 0.0f) p = 0.0f;
	if (p > 1.0f) p = 1.0f;
	cn->render_opacity     = p;
	cn->render_translate_y = (1.0f - p) * NAV_CONTENT_DY * d->content_dir;
}

static void nav_content_play(FluxNavViewData *d, int from, int to) {
	if (!d->content_ready || from < 0) return;
	d->content_dir = (to >= from) ? 1.0f : -1.0f;
	flux_nav_tween_snap(&d->content_t, 0.0f);
	nav_tween_to(&d->content_t, 1.0f, NAV_CONTENT_MS);
	nav_apply_content_motion(d);
}

static float nav_overlay_ease(float t) { return flux_cubic_bezier(t, 0.1f, 0.9f, 0.2f, 1.0f); }

static bool  nav_tick_one(FluxNavViewData *d, DWORD now) {
	bool active
	  = nav_tween_tick(&d->pane_progress, now, flux_nav_pane_overlay(d) ? nav_overlay_ease : flux_ease_out_cubic);
	active |= nav_tween_tick(&d->content_t, now, flux_ease_out_cubic);
	active |= nav_tick_expand(d, now);
	nav_apply_content_motion(d);
	nav_apply_geometry(d);
	flux_nav_fly_refresh(d);
	return nav_indicator_tick(d, now) || active || d->content_t.active;
}

static bool nav_step(void *ctx, unsigned long now) {
	FluxNavViewData *d      = ( FluxNavViewData * ) ctx;
	bool             active = nav_tick_one(d, ( DWORD ) now);
	flux_nav_repaint(d);
	if (!active) d->anim_active = false;
	return active;
}

void flux_nav_anim_start(FluxNavViewData *d) {
	d->anim_active = true;
	flux_anim_register(d, nav_step);
}

static FluxNavItemPlace nav_place_for(FluxNavViewData const *d, FluxNavViewItem const *it) {
	if (d->mode != FLUX_NAV_TOP) return FLUX_NAV_PLACE_LEFT;
	if (it->parent >= 0) return FLUX_NAV_PLACE_OVERFLOW;
	return it->in_overflow ? FLUX_NAV_PLACE_OVERFLOW : FLUX_NAV_PLACE_TOP;
}

static FluxUiaHooks const nav_uia_topitem = {
  .role          = FLUX_UIA_TABITEM,
  .patterns      = FLUX_UIA_P_EXPAND | FLUX_UIA_P_SELECTION_ITEM,
  .expand        = flux_nav_uia_expand,
  .toggle_expand = flux_nav_uia_toggle,
  .live          = flux_nav_uia_live,
  .setinfo       = flux_navitem_uia_setinfo};

static bool nav_inert(FluxNavViewItem const *it) {
	return it->kind == FLUX_NAV_ITEM_SEPARATOR || it->kind == FLUX_NAV_ITEM_HEADER;
}

static void nav_refresh_item(FluxNavViewData *d, FluxNavViewItem *it) {
	it->place        = ( uint8_t ) nav_place_for(d, it);
	FluxNodeData *nd = flux_store_find(d->store, it->node);
	if (nd && !nav_inert(it)) nd->uia = it->place == FLUX_NAV_PLACE_LEFT ? NULL : &nav_uia_topitem;
	if (it->place == FLUX_NAV_PLACE_TOP) {
		xent_setsize(d->ctx, it->node, (XentSize) {flux_nav_top_item_w(d, it), FLUX_NAV_TOP_H});
		return;
	}
	if (it->kind != FLUX_NAV_ITEM_HEADER) xent_setsize(d->ctx, it->node, (XentSize) {NAN, nav_item_height(it->kind)});
}

void flux_nav_refresh_items(FluxNavViewData *d) {
	for (int i = 0; i < d->count; i++) nav_refresh_item(d, &d->items [i]);
	if (d->settings != XENT_NODE_INVALID) nav_refresh_item(d, &d->settings_item);
	nav_sync_header_heights(d);
	flux_nav_sync_hosts(d);
}

static void nav_snap_children(FluxNavViewData *d) {
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (!it->has_children) continue;
		bool open = !flux_nav_flyout_repeater(d, it) && it->expanded;
		flux_nav_tween_snap(&it->expand_t, open ? 1.0f : 0.0f);
	}
	flux_nav_sync_hosts(d);
}

static void nav_set_pane_padding(FluxNavViewData *d) {
	if (d->mode == FLUX_NAV_TOP) {
		float left = FLUX_NAV_TOP_GRID_PAD + flux_nav_top_back_width(d);
		xent_setp(d->ctx, d->pane, (XentInsets) {left, 0.0f, FLUX_NAV_TOP_GRID_PAD, 0.0f});
		return;
	}
	float top_pad = d->window_title_bar ? nav_titlebar_height(d) : FLUX_NAV_GRID_MARGIN_Y;
	xent_setp(
	  d->ctx, d->pane,
	  (XentInsets) {NAV_PANE_PAD_X, top_pad + nav_back_row_height(d), NAV_PANE_PAD_X, FLUX_NAV_GRID_MARGIN_Y}
	);
}

static void nav_expansion_memory(FluxNavViewData *d) {
	bool open = d->pane_open;
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (it->parent >= 0 || !it->has_children) continue;
		if (d->mode == FLUX_NAV_TOP) {
			it->restore_expanded = false;
			continue;
		}
		if (open == it->expanded || (open && !it->restore_expanded)) continue;
		it->restore_expanded = !open;
		it->expanded         = open;
	}
}

static void nav_focus_open_pane(FluxNavViewData *d) {
	if (!d->input) return;
	d->pane_previous_focus       = flux_input_focus(d->input);
	FluxNodeData const *previous = flux_store_find(d->store, d->pane_previous_focus);
	d->pane_previous_suppressed  = !previous || previous->state.suppress_focus;
	flux_input_focuswithin(d->input, d->pane_clip, d->pane_previous_suppressed);
}

static void nav_restore_pane_focus(FluxNavViewData *d) {
	if (!d->input) return;
	XentNodeId previous    = d->pane_previous_focus;
	d->pane_previous_focus = XENT_NODE_INVALID;
	if (previous != XENT_NODE_INVALID
	    && !xent_collapsed(d->ctx, previous)
	    && flux_input_focuswithin(d->input, previous, d->pane_previous_suppressed))
		return;
	XentNodeId focus = flux_input_focus(d->input);
	if (focus != d->close && !input_within(d->ctx, focus, d->pane_clip)) return;
	if (!flux_input_focuswithin(d->input, d->content, d->pane_previous_suppressed))
		flux_input_setfocus(d->input, XENT_NODE_INVALID, false);
}

static void nav_animate_pane(FluxNavViewData *d) {
	float target = d->pane_open ? 1.0f : 0.0f;
	if (!nav_motion_enabled()) {
		flux_nav_tween_snap(&d->pane_progress, target);
		return;
	}
	float duration = NAV_PANE_ANIM_MS;
	if (flux_nav_pane_overlay(d)) duration = d->pane_open ? NAV_OVERLAY_OPEN_MS : NAV_OVERLAY_CLOSE_MS;
	nav_tween_to(&d->pane_progress, target, duration);
}

static void nav_set_pane_open(FluxNavViewData *d, bool open) {
	if (d->pane_open == open) return;
	flux_nav_fly_close(d);
	d->pane_open = open;
	nav_animate_pane(d);
	nav_expansion_memory(d);
	nav_snap_children(d);
	nav_apply_geometry(d);
	if (flux_nav_pane_overlay(d)) {
		flux_input_set_dismiss_layer(d->input, d->root, open ? d->scrim : XENT_NODE_INVALID);
		if (open) {
			d->pane_viewport = flux_window_size(d->window);
			nav_focus_open_pane(d);
		}
		else nav_restore_pane_focus(d);
	}
	flux_nav_anim_start(d);
}

static void nav_apply_mode(FluxNavViewData *d) {
	flux_nav_fly_close(d);
	nav_expansion_memory(d);
	flux_nav_top_layout(d, d->mode == FLUX_NAV_TOP);
	nav_set_pane_padding(d);
	flux_nav_chrome_sample(d);
	flux_nav_refresh_items(d);
	nav_snap_children(d);

	nav_indicator_reset(d);
}

static void nav_set_mode(FluxNavViewData *d, FluxNavDisplayMode mode) {
	if (d->mode == mode) return;
	bool old_overlay = flux_nav_pane_overlay(d) && d->pane_open;
	bool keep_open   = mode == FLUX_NAV_COMPACT && d->mode == FLUX_NAV_MINIMAL && d->pane_open;
	d->mode          = mode;
	d->pane_open     = keep_open || (mode == FLUX_NAV_EXPANDED && !d->pane_force_closed);
	flux_nav_tween_snap(&d->pane_progress, d->pane_open ? 1.0f : 0.0f);
	nav_apply_mode(d);
	nav_apply_geometry(d);
	if (old_overlay && !(flux_nav_pane_overlay(d) && d->pane_open)) {
		flux_input_set_dismiss_layer(d->input, d->root, XENT_NODE_INVALID);
		nav_restore_pane_focus(d);
	}
	flux_nav_anim_start(d);
}

static void nav_toggle_click(void *ctx) {
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (d->mode == FLUX_NAV_TOP) return;
	d->pane_force_closed = d->pane_open;
	nav_set_pane_open(d, !d->pane_open);
}

static void nav_back_click(void *ctx) {
	FluxNavViewData *d = ctx;
	if (!nav_back_visible(d) || !xent_sem_enabled(d->ctx, d->back)) return;
	XtkHost *host = flux_store_host(d->store);
	if (host)
		flux_input_route_event(host, d->root, XTK_EVENT_BACK_REQUESTED, (XtkEventPayload) {0}, XTK_EVENT_CAUSE_USER);
}

static void nav_close_click(void *ctx) {
	FluxNavViewData *d = ctx;
	if (!nav_close_visible(d)) return;
	d->pane_force_closed = true;
	nav_set_pane_open(d, false);
}

static bool nav_escape_key(void *ctx, FluxKey key, bool down) {
	if (!down || key != FLUX_KEY_ESCAPE) return false;
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (!flux_nav_pane_overlay(d) || !d->pane_open) return false;
	nav_set_pane_open(d, false);
	return true;
}

static bool nav_item_key(void *ctx, FluxKey key, bool down) {
	FluxNavViewItem *it = ctx;
	if (flux_nav_top_key(it, key, down)) return true;
	return nav_escape_key(it->nav, key, down);
}

static void nav_clear_selected(FluxNavViewData *d) {
	FluxNavViewItem *it = nav_index_item(d, d->selected);
	if (it) xent_sem_setselected(d->ctx, it->node, false);
}

static void nav_mark_selected(FluxNavViewData *d, int index) {
	if (index == FLUX_NAV_SETTINGS && !d->settings_visible) return;
	FluxNavViewItem *it = nav_index_item(d, index);
	if (it) xent_sem_setselected(d->ctx, it->node, true);
}

void flux_nav_apply_selection(FluxNavViewData *d, int index, bool notify) {
	bool settings = index == FLUX_NAV_SETTINGS;
	if (!settings && (index < 0 || index >= d->count)) return;
	if (d->selected == index) return;
	int from = d->selected;
	nav_clear_selected(d);
	d->selected = index;
	nav_mark_selected(d, index);
	if (!settings) flux_nav_top_bring(d, index);
	nav_content_play(d, from, index);
	flux_nav_anim_start(d);
	d->selection_revision++;
	if (notify && d->on_selection_changed) d->on_selection_changed(d->on_sel_ctx, index);
}

static void nav_item_click(void *ctx) {
	FluxNavViewItem *it  = ( FluxNavViewItem * ) ctx;
	FluxNavViewData *d   = it->nav;
	bool expand_only     = it->press_on_chevron || (flux_nav_closed_compact(d) && flux_nav_flyout_repeater(d, it));
	it->press_on_chevron = false;
	if (!expand_only) flux_nav_apply_selection(d, it->index, true);
	if (it->has_children) nav_toggle_expand(d, it->index);
	else {
		if (flux_nav_pane_overlay(d)) nav_set_pane_open(d, false);
		flux_nav_fly_close(d);
	}
}

static void nav_item_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxNavViewItem *it = ( FluxNavViewItem * ) ctx;
	XentRect         r  = {0};
	if (!it->has_children || !xent_layout_rect(it->nav->ctx, it->node, &r)) return;
	if (it->place == FLUX_NAV_PLACE_TOP) {
		float end            = r.w - FLUX_NAV_ITEM_MARGIN_H;
		it->press_on_chevron = x >= end - NAV_CHEVRON_ZONE && x < end && y >= 2.0f && y < r.h - 2.0f;
		return;
	}
	it->press_on_chevron = x >= r.w - NAV_CHEVRON_ZONE;
}

static void nav_scrim_click(void *ctx) {
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (flux_nav_pane_overlay(d)) nav_set_pane_open(d, false);
}

static FluxUiaHooks const nav_uia_repeater = {.role = FLUX_UIA_GROUP};

static void               nav_make_children_host(FluxNavViewData *d, FluxNavViewItem *par) {
	XentNodeId container = xent_node_parent(d->ctx, par->node);
	par->children_host   = flux_factory_create_node(d->ctx, d->store, container, FLUX_CONTROL_CONTAINER);
	XentNodeId next      = xent_node_next(d->ctx, par->node);
	if (next != par->children_host) xent_node_move(d->ctx, container, par->children_host, next);
	xent_setproto(d->ctx, par->children_host, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, par->children_host, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, par->children_host, XENT_FLEX_ALIGN_STRETCH);
	xent_setshrink(d->ctx, par->children_host, 0.0f);
	xent_setcollapsed(d->ctx, par->children_host, true);
	FluxNodeData *hn = flux_store_find(d->store, par->children_host);
	if (hn) hn->clips_children = true;
	if (hn) hn->uia = &nav_uia_repeater;
	flux_nav_graft_host(d, par, true);
	nav_tween_init(&par->expand_t, 0.0f);
}

static void nav_toggle_expand(FluxNavViewData *d, int idx) {
	FluxNavViewItem *it = &d->items [idx];
	if (!it->has_children) return;
	if (flux_nav_flyout_repeater(d, it)) {
		if (d->fly_owner == idx) flux_nav_fly_close(d);
		else flux_nav_fly_open(d, idx);
		return;
	}
	it->expanded = !it->expanded;
	nav_tween_to(&it->expand_t, it->expanded ? 1.0f : 0.0f, NAV_EXPAND_MS);
	flux_nav_sync_hosts(d);
	flux_nav_anim_start(d);
}

static FluxNavDisplayMode nav_mode_for_width(float w) {
	if (w > 0.0f && w < NAV_ADAPT_COMPACT) return FLUX_NAV_MINIMAL;
	if (w < NAV_ADAPT_EXPANDED) return FLUX_NAV_COMPACT;
	return FLUX_NAV_EXPANDED;
}

static FluxNavDisplayMode nav_pane_mode(int pane, FluxNavDisplayMode fallback) {
	switch (pane) {
	case FLUX_NAV_PANE_LEFT         : return FLUX_NAV_EXPANDED;
	case FLUX_NAV_PANE_TOP          : return FLUX_NAV_TOP;
	case FLUX_NAV_PANE_LEFT_COMPACT : return FLUX_NAV_COMPACT;
	case FLUX_NAV_PANE_LEFT_MINIMAL : return FLUX_NAV_MINIMAL;
	default                         : return fallback;
	}
}

static float nav_dip_scale(FluxNavViewData *d) {
	FluxDpi dpi = flux_window_dpi(d->window);
	return dpi.dpi_x > 0.0f ? dpi.dpi_x / 96.0f : 1.0f;
}

static void nav_dismiss_after_resize(FluxNavViewData *d, bool resized) {
	if (!flux_nav_pane_overlay(d) || !d->pane_open) return;
	FluxSize viewport = flux_window_size(d->window);
	if (resized || viewport.w != d->pane_viewport.w || viewport.h != d->pane_viewport.h) nav_set_pane_open(d, false);
}

static void nav_clear_top_widths(FluxNavViewData *d) {
	for (int i = 0; i < d->count; i++) d->items [i].top_w = 0.0f;
}

static void nav_apply_width(FluxNavViewData *d) {
	FluxNavDisplayMode mode = d->adaptive ? nav_mode_for_width(d->width) : d->mode;
	if (mode != d->mode) nav_set_mode(d, mode);
	else {
		nav_apply_geometry(d);
		flux_nav_anim_start(d);
	}
}

uint32_t flux_nav_frame(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) {
	( void ) store;
	FluxNavViewData *d    = nd->component_data;
	d->content_ready = true;
	XentRect         rect = {0};
	if (node != d->root || !xent_layout_rect(ctx, node, &rect)) return 0u;
	bool  resized = rect.w != d->width || rect.h != d->height;
	float scale   = nav_dip_scale(d);
	bool  rescale = scale != d->layout_scale;
	nav_dismiss_after_resize(d, resized);
	if (rescale) {
		d->layout_scale = scale;
		nav_clear_top_widths(d);
	}
	bool widths        = flux_nav_chrome_sample(d);
	bool content_slots = nav_content_slot_layout(d, rect.w);
	if (!resized && !rescale && !widths && !content_slots) return 0u;
	if (resized) {
		d->width  = rect.w;
		d->height = rect.h;
	}
	nav_apply_width(d);
	return FLUX_FRAME_REDRAW;
}

static void nav_destroy(void *component_data) {
	FluxNavViewData *d = ( FluxNavViewData * ) component_data;
	if (!d) return;
	flux_nav_fly_destroy(d);
	nav_anim_remove(d);
	flux_str_free(d->app_title);
	flux_str_free(d->pane_title);
	flux_str_free(d->settings_item.label);
	flux_str_free(d->settings_item.icon_name);
	for (int i = 0; i < d->count; i++) {
		flux_str_free(d->items [i].label);
		flux_str_free(d->items [i].icon_name);
	}
	free(d);
}

static void nav_setup_pane(FluxNavViewData *d) {
	xent_setproto(d->ctx, d->pane, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->pane, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, d->pane, XENT_FLEX_ALIGN_STRETCH);
	nav_set_pane_padding(d);

	d->items_scroll = flux_scroll_init(&(FluxContainerDesc) {d->ctx, d->store, d->pane});
	xent_setitems(d->ctx, d->items_scroll, XENT_FLEX_ALIGN_STRETCH);
	xent_setgrow(d->ctx, d->items_scroll, 1.0f);
	xent_setbasis(d->ctx, d->items_scroll, 0.0f);
	xent_setshrink(d->ctx, d->items_scroll, 0.0f);
	FluxNodeData *sn     = flux_store_find(d->store, d->items_scroll);
	d->items_scroll_data = sn ? ( struct FluxScrollData * ) sn->component_data : NULL;

	d->items_host        = flux_factory_create_node(d->ctx, d->store, d->items_scroll, FLUX_CONTROL_CONTAINER);
	xent_setproto(d->ctx, d->items_host, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->items_host, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, d->items_host, XENT_FLEX_ALIGN_STRETCH);
	xent_setgap(d->ctx, d->items_host, FLUX_NAV_ITEM_GAP);
	xent_setfit(d->ctx, d->items_host, false, true);
	xent_setshrink(d->ctx, d->items_host, 0.0f);

	d->footer_host = flux_factory_create_node(d->ctx, d->store, d->pane, FLUX_CONTROL_CONTAINER);
	xent_setproto(d->ctx, d->footer_host, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->footer_host, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, d->footer_host, XENT_FLEX_ALIGN_STRETCH);
	xent_setgap(d->ctx, d->footer_host, FLUX_NAV_ITEM_GAP);
	xent_setfit(d->ctx, d->footer_host, false, true);
	xent_setshrink(d->ctx, d->footer_host, 0.0f);
	d->footer_scroll = flux_scroll_init(&(FluxContainerDesc) {d->ctx, d->store, d->pane});
	xent_setitems(d->ctx, d->footer_scroll, XENT_FLEX_ALIGN_STRETCH);
	xent_setshrink(d->ctx, d->footer_scroll, 0.0f);
	xent_seth(d->ctx, d->footer_scroll, 0.0f);
	d->pane_separator = flux_factory_create_node(d->ctx, d->store, d->pane, FLUX_CONTROL_CONTAINER);
	xent_seth(d->ctx, d->pane_separator, 1.0f);
	xent_setm(d->ctx, d->pane_separator, (XentInsets) {-NAV_PANE_PAD_X, 3.0f, -NAV_PANE_PAD_X, 6.0f});
	xent_setshrink(d->ctx, d->pane_separator, 0.0f);
	xent_setcollapsed(d->ctx, d->pane_separator, true);
	nav_bind_tab_group(d, d->items_host);
	nav_bind_tab_group(d, d->footer_host);
	flux_nav_top_setup(d);
	flux_nav_chrome_setup(d);
	flux_nav_settings_setup(d);
}

static void nav_toggle_name(FluxStore *store, XentNodeId node, char *out, size_t size) {
	FluxNodeData const    *nd = flux_store_find(store, node);
	FluxNavViewItem const *it = nd ? ( FluxNavViewItem const * ) nd->component_data : NULL;
	bool                   on = it && it->nav && it->nav->pane_open;
	snprintf(out, size, "%s", on ? "Close Navigation" : "Open Navigation");
}

static FluxUiaHooks const nav_uia_toggle = {
  .role     = FLUX_UIA_BUTTON,
  .patterns = FLUX_UIA_P_INVOKE,
  .invoked  = true,
  .landmark = FLUX_UIA_LANDMARK_NAVIGATION,
  .klass    = L"Button",
  .name     = nav_toggle_name};
static FluxUiaHooks const nav_uia_header = {.role = FLUX_UIA_TEXT, .klass = L"TextBlock"};
static FluxUiaHooks const nav_uia_hidden = {.role = FLUX_UIA_HIDDEN};
static FluxUiaHooks const nav_uia_main
  = {.role = FLUX_UIA_GROUP, .landmark = FLUX_UIA_LANDMARK_MAIN, .klass = L"LandmarkTarget"};
static FluxUiaHooks const nav_uia_pane = {.role = FLUX_UIA_WINDOW, .klass = L"SplitViewPane"};

static void               nav_make_toggle(FluxNavViewData *d) {
	d->toggle = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_NAV_VIEW_ITEM);
	xent_setproto(d->ctx, d->toggle, XENT_PROTOCOL_ABSOLUTE);
	xent_setpos(d->ctx, d->toggle, (XentPoint) {FLUX_NAV_ITEM_MARGIN_H, FLUX_NAV_PANE_TOP_PAD});
	xent_setsize(d->ctx, d->toggle, (XentSize) {FLUX_NAV_TOGGLE_W, FLUX_NAV_TOGGLE_H});

	FluxNodeData *tn = flux_store_find(d->store, d->toggle);
	if (tn) {
		d->toggle_item            = (FluxNavViewItem) {d, d->toggle, NULL, "GlobalNavButton", FLUX_NAV_ITEM_TOGGLE, -1};
		tn->component_data        = &d->toggle_item;
		tn->behavior.on_click     = nav_toggle_click;
		tn->behavior.on_click_ctx = d;
		tn->behavior.on_key       = nav_escape_key;
		tn->behavior.on_key_ctx   = d;
		tn->uia                   = &nav_uia_toggle;
	}
	xent_set_focusable(d->ctx, d->toggle, true);
}

static XentNodeId nav_make_titlebar(FluxNavViewData *d, char const *title) {
	return flux_titlebar_init(&(FluxTitleBarDesc) {
	  .ctx              = d->ctx,
	  .store            = d->store,
	  .parent           = d->root,
	  .window           = d->window,
	  .title            = title,
	  .show_pane_toggle = d->mode != FLUX_NAV_TOP,
	  .integrate_window = true,
	  .on_pane_toggle   = nav_toggle_click,
	  .pane_toggle_ctx  = d});
}

static XentNodeId nav_make_back_button(FluxNavViewData *d, bool close) {
	XentNodeId    node = flux_button_init(&(FluxButtonDesc) {
	  d->ctx, d->store, d->root, NULL, close ? nav_close_click : nav_back_click, d});
	FluxNodeData *nd   = flux_store_find(d->store, node);
	if (!nd) return XENT_NODE_INVALID;
	flux_prop_button_icon(d->store, node, "Back");
	flux_prop_button_style(d->store, node, FLUX_BUTTON_SUBTLE);
	flux_prop_button_enabled(d->store, node, close);
	xent_sem_setlabel(d->ctx, node, close ? "Close" : "Back");
	flux_uia_settext(d->store, node, FLUX_A11Y_ID, close ? "NavigationViewCloseButton" : "NavigationViewBackButton");
	nd->tooltip_text        = flux_str_dup(close ? "Close Navigation" : "Back");
	FluxButtonData *button  = nd->component_data;
	button->navigation_back = true;
	return node;
}

static void nav_init_data(FluxNavViewData *d, FluxNavViewDesc const *info, XentNodeId root) {
	d->ctx      = info->ctx;
	d->store    = info->store;
	d->window   = info->window;
	d->text     = info->text;
	d->input    = info->input;
	d->root     = root;
	d->selected = -1;

	nav_indicator_reset(d);
	d->adaptive             = info->adaptive || info->pane == FLUX_NAV_PANE_AUTO;
	d->mode                 = ( FluxNavDisplayMode ) info->mode;
	d->pane_mode            = info->pane;
	d->width                = info->width;
	d->height               = info->height;
	d->window_title_bar     = info->window_title_bar;
	d->app_title            = info->app_title ? flux_str_dup(info->app_title) : NULL;
	d->title_bar            = XENT_NODE_INVALID;
	d->theme                = info->theme;
	d->fly_owner            = FLUX_NAV_FLY_NONE;
	d->fly_root             = XENT_NODE_INVALID;
	d->fly_scroll           = XENT_NODE_INVALID;
	d->overflow_host        = XENT_NODE_INVALID;
	d->on_selection_changed = info->on_selection_changed;
	d->on_sel_ctx           = info->userdata;
	d->settings_visible     = true;
	d->always_show_header   = true;
	nav_tween_init(&d->pane_progress, 0.0f);
	nav_tween_init(&d->content_t, 1.0f);
	d->content_dir   = 1.0f;
	d->content_ready = false;
}

static bool nav_make_pane(FluxNavViewData *d) {
	d->pane_clip       = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_NAV_VIEW);
	FluxNodeData *clip = flux_store_find(d->store, d->pane_clip);
	if (!clip) return false;
	clip->component_data = d;
	clip->clips_children = true;
	clip->uia            = &nav_uia_pane;
	xent_setproto(d->ctx, d->pane_clip, XENT_PROTOCOL_ABSOLUTE);
	flux_uia_settext(d->store, d->pane_clip, FLUX_A11Y_ID, "PaneRoot");

	d->pane            = flux_factory_create_node(d->ctx, d->store, d->pane_clip, FLUX_CONTROL_CONTAINER);
	FluxNodeData *pane = flux_store_find(d->store, d->pane);
	if (!pane) return false;
	pane->uia = &nav_uia_hidden;
	nav_setup_pane(d);
	return true;
}

static XentNodeId nav_content_container(FluxNavViewData *d, XentNodeId parent, FluxUiaHooks const *uia) {
	XentNodeId node = flux_factory_create_node(d->ctx, d->store, parent, FLUX_CONTROL_CONTAINER);
	xent_setproto(d->ctx, node, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, node, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, node, XENT_FLEX_ALIGN_STRETCH);
	flux_uia_part_hooks(d->store, node, uia);
	return node;
}

static bool nav_make_content(FluxNavViewData *d) {
	d->content_grid = nav_content_container(d, d->root, &nav_uia_hidden);
	if (!d->content_grid) return false;
	d->page_header = nav_content_container(d, d->content_grid, &nav_uia_hidden);
	xent_setfit(d->ctx, d->page_header, false, true);
	xent_setminsize(d->ctx, d->page_header, (XentSize) {0.0f, FLUX_NAV_TOGGLE_H});
	xent_setshrink(d->ctx, d->page_header, 0.0f);
	xent_setcollapsed(d->ctx, d->page_header, true);
	d->content = flux_factory_create_node(d->ctx, d->store, d->content_grid, FLUX_CONTROL_CONTAINER);
	flux_uia_part_hooks(d->store, d->content, &nav_uia_main);
	FluxNodeData *cn = flux_store_find(d->store, d->content);
	if (cn) cn->clips_children = true;
	xent_setgrow(d->ctx, d->content, 1.0f);
	xent_setbasis(d->ctx, d->content, 0.0f);
	return d->page_header && d->content;
}

static bool nav_build_tree(FluxNavViewData *d) {
	xent_setproto(d->ctx, d->root, XENT_PROTOCOL_ABSOLUTE);
	if (d->width > 0.0f) xent_setw(d->ctx, d->root, d->width);
	else xent_setwpct(d->ctx, d->root, 1.0f);
	if (d->height > 0.0f) xent_seth(d->ctx, d->root, d->height);
	else xent_sethpct(d->ctx, d->root, 1.0f);
	FluxNodeData *root = flux_store_find(d->store, d->root);
	if (root) root->tab_reverse_children = true;

	if (!nav_make_content(d)) return false;

	d->scrim          = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_CONTAINER);
	FluxNodeData *scn = flux_store_find(d->store, d->scrim);
	if (scn) {
		scn->behavior.on_click     = nav_scrim_click;
		scn->behavior.on_click_ctx = d;
	}

	if (!nav_make_pane(d)) return false;
	d->content_overlay = nav_content_container(d, d->root, &nav_uia_hidden);
	if (!d->content_overlay) return false;
	nav_make_toggle(d);
	d->back  = nav_make_back_button(d, false);
	d->close = nav_make_back_button(d, true);
	if (!d->back || !d->close) return false;

	if (d->window_title_bar) d->title_bar = nav_make_titlebar(d, d->app_title);
	return true;
}

void flux_nav_set_always_show_header(FluxStore *store, XentNodeId nav, bool always) {
	FluxNavViewData *d = flux_nav_data(store, nav);
	if (!d || d->always_show_header == always) return;
	d->always_show_header = always;
	nav_apply_geometry(d);
}

void flux_nav_set_back_button(FluxStore *store, XentNodeId nav, uint32_t visibility) {
	FluxNavViewData *d = flux_nav_data(store, nav);
	if (!d || visibility > XTK_NAV_BACK_COLLAPSED || d->back_button == visibility) return;
	d->back_button = visibility;
	nav_apply_geometry(d);
}

void flux_nav_set_back_enabled(FluxStore *store, XentNodeId nav, bool enabled) {
	FluxNavViewData *d = flux_nav_data(store, nav);
	if (d) flux_prop_button_enabled(store, d->back, enabled);
}

static void nav_apply_initial_geometry(FluxNavViewData *d) {
	FluxSize viewport = flux_window_size(d->window);
	float    scale    = nav_dip_scale(d);
	if (d->width <= 0.0f) d->width = viewport.w / scale;
	if (d->height <= 0.0f) d->height = viewport.h / scale;
	d->layout_scale = scale;
	d->mode         = d->adaptive ? nav_mode_for_width(d->width) : nav_pane_mode(d->pane_mode, d->mode);
	d->pane_open    = d->mode == FLUX_NAV_EXPANDED;
	flux_nav_tween_snap(&d->pane_progress, d->pane_open ? 1.0f : 0.0f);
	nav_apply_mode(d);
	nav_apply_geometry(d);
}

XentNodeId flux_nav_init(FluxNavViewDesc const *info) {
	if (!info || !info->ctx || !info->store || !info->window) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_NAV_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData    *nd = flux_store_find(info->store, root);
	FluxNavViewData *d  = nd ? ( FluxNavViewData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	nav_init_data(d, info, root);
	nd->component_data         = d;
	nd->destroy_component_data = nav_destroy;
	nd->clips_children         = true;
	if (!nav_build_tree(d)) return flux_factory_fail_node(info->ctx, info->store, root);
	nav_apply_initial_geometry(d);

	if (info->out_content) *info->out_content = d->content;
	return root;
}

void flux_nav_set_pane_mode(FluxStore *store, XentNodeId nav, int pane_mode) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d || d->pane_mode == pane_mode) return;
	d->pane_mode         = pane_mode;
	d->adaptive          = pane_mode == FLUX_NAV_PANE_AUTO;
	d->pane_force_closed = false;
	nav_set_mode(d, d->adaptive ? nav_mode_for_width(d->width) : nav_pane_mode(pane_mode, d->mode));
	if (d->mode == FLUX_NAV_EXPANDED) nav_set_pane_open(d, true);
}

static bool nav_update_titlebar(FluxNavViewData *d, bool window_title_bar, char const *title) {
	if (window_title_bar && d->title_bar == XENT_NODE_INVALID) {
		d->title_bar = nav_make_titlebar(d, title);
		return d->title_bar != XENT_NODE_INVALID;
	}
	if (!window_title_bar && d->title_bar != XENT_NODE_INVALID) {
		flux_tree_drop(d->store, d->title_bar);
		d->title_bar = XENT_NODE_INVALID;
		flux_window_extendtitlebar(d->window, false);
	}
	if (d->title_bar != XENT_NODE_INVALID) flux_titlebar_set_title(d->store, d->title_bar, title, NULL);
	return true;
}

bool flux_nav_set_chrome(FluxStore *store, XentNodeId nav, uint32_t flags, char const *app_title) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d) return false;
	char *title = flux_str_dup(app_title);
	if (app_title && !title) return false;
	bool window_title_bar = (flags & XTK_NAV_MODEL_WINDOW_TITLE_BAR) != 0u;
	if (!nav_update_titlebar(d, window_title_bar, title)) {
		flux_str_free(title);
		return false;
	}
	flux_str_free(d->app_title);
	d->app_title        = title;
	d->window_title_bar = window_title_bar;
	d->tall_title_bar   = (flags & XTK_NAV_MODEL_TALL_TITLE_BAR) != 0u;
	nav_set_pane_padding(d);
	nav_apply_geometry(d);
	return true;
}

static float nav_item_height(FluxNavItemKind kind) {
	if (kind == FLUX_NAV_ITEM_SEPARATOR) return FLUX_NAV_SEP_H;
	if (kind == FLUX_NAV_ITEM_HEADER) return FLUX_NAV_HEADER_H;
	return FLUX_NAV_ITEM_HEIGHT;
}

static void
nav_inert_uia(FluxNavViewData *d, FluxNodeData *in, XentNodeId node, FluxNavItemKind kind, char const *label) {
	if (in) in->uia = kind == FLUX_NAV_ITEM_HEADER ? &nav_uia_header : &nav_uia_hidden;
	if (label) xent_sem_setlabel(d->ctx, node, label);
}

static void nav_item_focus(void *ctx) {
	FluxNavViewItem const *it   = ctx;
	FluxUiaContext         info = {.ctx = it->nav->ctx, .store = it->nav->store};
	flux_uia_bring_into_view(&info, it->node);
}

static void nav_bind_item(FluxNavViewData *d, FluxNavViewItem *slot, FluxNodeData *in) {
	in->component_data               = slot;
	in->behavior.on_click            = nav_item_click;
	in->behavior.on_click_ctx        = slot;
	in->behavior.on_pointer_down     = nav_item_down;
	in->behavior.on_pointer_down_ctx = slot;
	in->behavior.on_key              = nav_item_key;
	in->behavior.on_key_ctx          = slot;
	in->behavior.on_focus            = nav_item_focus;
	in->behavior.on_focus_ctx        = slot;
	xent_set_focusable(d->ctx, slot->node, true);
	xent_sem_setrole(d->ctx, slot->node, XENT_SEM_ROLE_BUTTON);
	if (slot->label) xent_sem_setlabel(d->ctx, slot->node, slot->label);
}

static void nav_setup_item(FluxNavViewData *d, FluxNavViewItem *slot, char const *label) {
	FluxNodeData *in = flux_store_find(d->store, slot->node);
	if (!in) return;
	if (nav_inert(slot)) {
		in->component_data = slot;
		nav_inert_uia(d, in, slot->node, slot->kind, label);
		return;
	}
	nav_bind_item(d, slot, in);
}

static void nav_move_focus_off_settings(FluxNavViewData *d) {
	if (!d->input || !input_within(d->ctx, flux_input_focus(d->input), d->settings)) return;
	if (!flux_input_focuswithin(d->input, d->content, false)) flux_input_setfocus(d->input, XENT_NODE_INVALID, false);
}

void flux_nav_settings_setup(FluxNavViewData *d) {
	d->settings_visible = true;
	d->settings         = flux_factory_create_node(d->ctx, d->store, d->footer_host, FLUX_CONTROL_NAV_VIEW_ITEM);
	if (d->settings == XENT_NODE_INVALID) return;
	d->settings_item = (FluxNavViewItem) {
	  d, d->settings, flux_str_dup("Settings"), flux_str_dup("Settings"), FLUX_NAV_ITEM_SETTINGS, FLUX_NAV_SETTINGS};
	d->settings_item.parent        = -1;
	d->settings_item.children_host = XENT_NODE_INVALID;
	nav_setup_item(d, &d->settings_item, "Settings");
	flux_uia_settext(d->store, d->settings, FLUX_A11Y_ID, "SettingsItem");
	xent_setshrink(d->ctx, d->settings, 0.0f);
	nav_refresh_item(d, &d->settings_item);
}

void flux_nav_set_settings_visible(FluxStore *store, XentNodeId nav, bool visible) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d || d->settings == XENT_NODE_INVALID || d->settings_visible == visible) return;
	d->settings_visible = visible;
	xent_setcollapsed(d->ctx, d->settings, !visible);
	if (!visible) nav_move_focus_off_settings(d);
	if (!visible && d->selected == FLUX_NAV_SETTINGS) xent_sem_setselected(d->ctx, d->settings, false);
	if (visible && d->selected == FLUX_NAV_SETTINGS) xent_sem_setselected(d->ctx, d->settings, true);
	flux_nav_anim_start(d);
	flux_nav_chrome_sync(d);
}

void flux_nav_open_for_search(FluxNavViewData *d) {
	if (!d) return;
	d->pane_force_closed = false;
	nav_set_pane_open(d, true);
	if (d->input) flux_input_focuswithin(d->input, d->chrome_search, false);
}

void flux_nav_on_query(FluxStore *store, XentNodeId origin, int chosen) {
	if (chosen < 0) return;
	XentCtx *ctx = flux_store_ctx(store);
	for (XentNodeId p = origin; p != XENT_NODE_INVALID; p = xent_node_parent(ctx, p)) {
		FluxNavViewData *d = nav_data(store, p);
		if (!d || p != d->root) continue;
		if (d->mode != FLUX_NAV_EXPANDED && input_within(ctx, origin, d->chrome_search)) nav_set_pane_open(d, false);
		return;
	}
}

static void nav_keep_settings_last(FluxNavViewData *d, XentNodeId host) {
	if (host == d->footer_host && d->settings != XENT_NODE_INVALID)
		flux_nav_place(d, d->footer_host, d->settings, XENT_NODE_INVALID);
}

static int nav_add(FluxNavViewData *d, XentNodeId host, char const *icon, char const *label, FluxNavItemKind kind) {
	if (d->count >= FLUX_NAV_VIEW_MAX_ITEMS) {
		OutputDebugStringA("fluxent: NavigationView item capacity exceeded; item dropped\n");
		return -1;
	}
	XentNodeId node = flux_factory_create_node(d->ctx, d->store, host, FLUX_CONTROL_NAV_VIEW_ITEM);
	if (node == XENT_NODE_INVALID) return -1;
	xent_setsize(d->ctx, node, (XentSize) {NAN, nav_item_height(kind)});
	xent_setshrink(d->ctx, node, 0.0f);

	FluxNavViewItem *slot = &d->items [d->count];
	*slot                 = (FluxNavViewItem) {d, node, flux_str_dup(label), flux_str_dup(icon), kind, d->count};
	slot->parent          = -1;
	slot->children_host   = XENT_NODE_INVALID;
	slot->footer          = host == d->footer_host;

	nav_setup_item(d, slot, label);

	int index = d->count++;
	nav_refresh_item(d, slot);
	nav_keep_settings_last(d, host);
	if (d->selected == -1 && kind == FLUX_NAV_ITEM_MENU) flux_nav_apply_selection(d, index, false);
	if (d->mode == FLUX_NAV_TOP) flux_nav_anim_start(d);
	return index;
}

static void nav_destroy_item_nodes(FluxNavViewData *d) {
	for (int i = d->count - 1; i >= 0; i--)
		if (d->items [i].children_host != XENT_NODE_INVALID) xent_node_destroy(d->ctx, d->items [i].children_host);
	for (int i = 0; i < d->count; i++)
		if (xent_node_valid(d->ctx, d->items [i].node)) xent_node_destroy(d->ctx, d->items [i].node);
}

bool flux_nav_reset_items(FluxStore *store, XentNodeId nav) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d) return false;
	flux_nav_fly_close(d);
	nav_anim_remove(d);
	d->content_ready = false;
	flux_nav_tween_snap(&d->content_t, 1.0f);
	nav_apply_content_motion(d);
	nav_destroy_item_nodes(d);
	for (int i = 0; i < d->count; i++) {
		flux_str_free(d->items [i].label);
		flux_str_free(d->items [i].icon_name);
	}
	memset(d->items, 0, sizeof(d->items));
	d->count    = 0;
	d->selected = -1;
	if (d->settings != XENT_NODE_INVALID) {
		xent_sem_setselected(d->ctx, d->settings, false);
		flux_nav_place(d, d->footer_host, d->settings, XENT_NODE_INVALID);
	}
	nav_indicator_reset(d);
	d->top_more = false;
	return true;
}

int flux_nav_add_item(FluxStore *store, XentNodeId nav, char const *icon_name, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->items_host, icon_name, label, FLUX_NAV_ITEM_MENU) : -1;
}

int flux_nav_addfooter(FluxStore *store, XentNodeId nav, char const *icon_name, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->footer_host, icon_name, label, FLUX_NAV_ITEM_FOOTER) : -1;
}

int flux_nav_add_footer_header(FluxStore *store, XentNodeId nav, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->footer_host, NULL, label, FLUX_NAV_ITEM_HEADER) : -1;
}

int flux_nav_add_separator(FluxStore *store, XentNodeId nav) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->items_host, NULL, NULL, FLUX_NAV_ITEM_SEPARATOR) : -1;
}

int flux_nav_add_header(FluxStore *store, XentNodeId nav, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->items_host, NULL, label, FLUX_NAV_ITEM_HEADER) : -1;
}

int flux_nav_addchild(FluxStore *store, XentNodeId nav, int parent_index, char const *icon_name, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d || parent_index < 0 || parent_index >= d->count) return -1;
	FluxNavViewItem *par = &d->items [parent_index];
	if (par->kind != FLUX_NAV_ITEM_MENU && par->kind != FLUX_NAV_ITEM_FOOTER) return -1;
	if (!par->has_children) {
		par->has_children = true;
		par->top_w        = 0.0f;
		nav_make_children_host(d, par);
	}
	int idx = nav_add(d, par->children_host, icon_name, label, FLUX_NAV_ITEM_MENU);
	if (idx < 0) return -1;
	d->items [idx].parent = parent_index;
	d->items [idx].depth  = par->depth + 1;
	nav_refresh_item(d, &d->items [idx]);
	flux_nav_sync_hosts(d);
	return idx;
}

void flux_nav_select(FluxStore *store, XentNodeId nav, int index) {
	FluxNavViewData *d = nav_data(store, nav);
	if (d) flux_nav_apply_selection(d, index, true);
}

void flux_nav_set_selected(FluxStore *store, XentNodeId nav, int index) {
	FluxNavViewData *d = nav_data(store, nav);
	if (d) flux_nav_apply_selection(d, index, false);
}

XentNodeId flux_nav_content_node(FluxStore *store, XentNodeId nav) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? d->content : XENT_NODE_INVALID;
}

static FluxNavViewItem *nav_item_of(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && nd->behavior.on_click == nav_item_click ? ( FluxNavViewItem * ) nd->behavior.on_click_ctx : NULL;
}

static FluxNavItemKind nav_root_kind(FluxNavViewData const *d, FluxNavViewItem const *it) {
	while (it->parent >= 0) it = &d->items [it->parent];
	return it->kind;
}

static bool nav_same_set(FluxNavViewData const *d, FluxNavViewItem const *a, FluxNavViewItem const *b) {
	if (a->parent != b->parent || nav_root_kind(d, a) != nav_root_kind(d, b)) return false;
	return a->parent >= 0 || a->in_overflow == b->in_overflow;
}

static int nav_footer_roots(FluxNavViewData const *d) {
	int n = 0;
	for (int i = 0; i < d->count; i++)
		if (d->items [i].kind == FLUX_NAV_ITEM_FOOTER && d->items [i].parent < 0) n++;
	return n;
}

static void nav_count_settings(FluxNavViewData const *d, int *pos, int *size) {
	int n = nav_footer_roots(d);
	*pos  = n + 1;
	*size = n + (d->settings_visible ? 1 : 0);
}

static bool nav_count_skip(FluxNavViewData const *d, FluxNavViewItem const *o, FluxNavViewItem const *it) {
	return !nav_same_set(d, o, it) || o->kind == FLUX_NAV_ITEM_SEPARATOR;
}

static void nav_count_walk(FluxNavViewData const *d, FluxNavViewItem const *it, int *pos, int *size) {
	int  n     = 0;
	bool found = false;
	*pos       = 0;
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem const *o = &d->items [i];
		if (nav_count_skip(d, o, it)) continue;
		if (o->kind == FLUX_NAV_ITEM_HEADER && found) break;
		if (o->kind != FLUX_NAV_ITEM_HEADER) n++;
		if (o == it) *pos = n;
		found = found || o == it;
	}
	*size = n;
}

static bool nav_is_footer_root(FluxNavViewData const *d, FluxNavViewItem const *it) {
	return it->parent < 0 && nav_root_kind(d, it) == FLUX_NAV_ITEM_FOOTER;
}

static void nav_count_set(FluxNavViewData const *d, FluxNavViewItem const *it, int *pos, int *size) {
	if (it->kind == FLUX_NAV_ITEM_SETTINGS) {
		nav_count_settings(d, pos, size);
		return;
	}
	nav_count_walk(d, it, pos, size);
	if (d->settings_visible && nav_is_footer_root(d, it)) *size += 1;
}

bool flux_navitem_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level) {
	FluxNavViewItem *it = nav_item_of(store, node);
	if (!it || !it->nav) return false;
	nav_count_set(it->nav, it, pos, size);
	*level = it->depth + 1;
	return true;
}

int flux_nav_uia_expand(FluxStore *store, XentNodeId node) {
	FluxNavViewItem *it = nav_item_of(store, node);
	if (!it || !it->has_children) return 3;
	return it->expanded ? 1 : 0;
}

unsigned flux_nav_uia_live(FluxStore *store, XentNodeId node) {
	FluxNavViewItem *it = nav_item_of(store, node);
	return it && it->has_children ? ~0u : ~( unsigned ) FLUX_UIA_P_EXPAND;
}

void flux_nav_uia_toggle(FluxStore *store, XentNodeId node) {
	FluxNavViewItem *it = nav_item_of(store, node);
	if (it) nav_toggle_expand(it->nav, it->index);
}
