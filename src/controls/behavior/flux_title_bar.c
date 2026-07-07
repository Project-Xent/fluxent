/**
 * @file flux_title_bar.c
 * @brief TitleBar behavior: a leaf band with an optional back button,
 * pane-toggle button, icon, and a title/subtitle. Back and pane-toggle are
 * region hit-tested (shared with the renderer via flux_titlebar_layout). When
 * window integration is enabled, a per-frame sync reports the drag rect and the
 * interactive button rects to the window so the band moves the window while the
 * buttons remain clickable.
 */
#include "controls/factory/flux_factory.h"
#include "render/flux_icon.h"
#include "runtime/flux_str.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"

#include <stdlib.h>
#include <string.h>

static FluxTitleBarData *titlebar_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_TITLE_BAR || !nd->component_data) return NULL;
	return ( FluxTitleBarData * ) nd->component_data;
}

static void titlebar_measure_title(FluxTitleBarData *d) {
	d->title_w = 0.0f;
	if (!d->title || !d->title [0]) return;
	XentTextMetrics tm = {0};
	if (xent_measure_text(
	      d->ctx,
	      &(XentTextMeasureRequest) {
	        .text = d->title, .font_size = 14.0f, .line_break_policy = XENT_LINE_BREAK_NO_WRAP,
	        .width_mode = XENT_MEASURE_UNDEFINED},
	      &tm
	    ))
		d->title_w = tm.width;
}

static void titlebar_set_icon(FluxTitleBarData *d, char const *name) {
	d->icon_glyph [0] = '\0';
	if (!name || !name [0]) return;
	wchar_t const *cp = flux_icon_lookup(name);
	if (cp) flux_icon_to_utf8(cp, d->icon_glyph, sizeof(d->icon_glyph));
	else {
		size_t n = strlen(name);
		if (n < sizeof(d->icon_glyph)) memcpy(d->icon_glyph, name, n + 1);
	}
}

static bool tb_in(FluxRect const *r, float x, float y) {
	return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static int titlebar_hit(FluxTitleBarData const *d, float x, float y) {
	XentRect r = {0};
	xent_get_layout_rect(d->ctx, d->node, &r);
	FluxRect           b = {0.0f, 0.0f, r.w, r.h};
	FluxTitleBarLayout l = flux_titlebar_layout(&b, d->show_back, d->show_pane_toggle, d->icon_glyph [0] != '\0', d->integrate_window);
	if (l.has_back && !d->back_disabled && tb_in(&l.back, x, y)) return FLUX_TB_REGION_BACK;
	if (l.has_toggle && tb_in(&l.toggle, x, y)) return FLUX_TB_REGION_PANE;
	if (l.has_caption) {
		if (tb_in(&l.min, x, y)) return FLUX_TB_REGION_MIN;
		if (tb_in(&l.max, x, y)) return FLUX_TB_REGION_MAX;
		if (tb_in(&l.close, x, y)) return FLUX_TB_REGION_CLOSE;
	}
	return FLUX_TB_REGION_NONE;
}

/* Per-region tooltip text for the caption buttons (the custom frame removes the OS
 * ones). NULL elsewhere so the drag band shows no tooltip. */
static char const *titlebar_tooltip_at(void *ctx, float x, float y) {
	FluxTitleBarData *d = ( FluxTitleBarData * ) ctx;
	switch (titlebar_hit(d, x, y)) {
	case FLUX_TB_REGION_MIN  : return "Minimize";
	case FLUX_TB_REGION_MAX  : return d->maximized ? "Restore" : "Maximize";
	case FLUX_TB_REGION_CLOSE: return "Close";
	default                  : return NULL;
	}
}

static void titlebar_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxTitleBarData *d = ( FluxTitleBarData * ) ctx;
	d->pressed          = titlebar_hit(d, x, y);
}

/* Run the action bound to a region: back/pane callbacks, or the window command. */
static void titlebar_invoke(FluxTitleBarData *d, int region) {
	HWND hwnd = d->window ? flux_window_hwnd(d->window) : NULL;
	switch (region) {
	case FLUX_TB_REGION_BACK: if (d->on_back) d->on_back(d->on_back_ctx); break;
	case FLUX_TB_REGION_PANE: if (d->on_pane_toggle) d->on_pane_toggle(d->on_pane_toggle_ctx); break;
	case FLUX_TB_REGION_MIN: if (hwnd) ShowWindow(hwnd, SW_MINIMIZE); break;
	case FLUX_TB_REGION_MAX: if (hwnd) ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE); break;
	case FLUX_TB_REGION_CLOSE: if (hwnd) PostMessageW(hwnd, WM_CLOSE, 0, 0); break;
	default: break;
	}
}

static void titlebar_click(void *ctx) {
	FluxTitleBarData *d   = ( FluxTitleBarData * ) ctx;
	FluxNodeData     *nd  = ( FluxNodeData * ) xent_get_userdata(d->ctx, d->node);
	int               hit = nd ? titlebar_hit(d, nd->hover_local_x, nd->hover_local_y) : FLUX_TB_REGION_NONE;
	int               was = d->pressed;
	d->pressed            = FLUX_TB_REGION_NONE;
	/* Press and release must land on the same region for the action to fire. */
	if (hit == was && hit != FLUX_TB_REGION_NONE) titlebar_invoke(d, hit);
}

static void titlebar_cancel(void *ctx) {
	FluxTitleBarData *d = ( FluxTitleBarData * ) ctx;
	d->pressed          = FLUX_TB_REGION_NONE;
}

static void titlebar_destroy(void *component_data) {
	FluxTitleBarData *d = ( FluxTitleBarData * ) component_data;
	if (!d) return;
	flux_str_free(d->title);
	flux_str_free(d->subtitle);
	free(d);
}

XentNodeId flux_create_title_bar(FluxTitleBarCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_TITLE_BAR);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_node_store_get(info->store, node);
	FluxTitleBarData *d  = nd ? ( FluxTitleBarData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	d->ctx                = info->ctx;
	d->store              = info->store;
	d->node               = node;
	d->window             = info->window;
	d->title              = info->title ? flux_str_dup(info->title) : NULL;
	d->subtitle           = info->subtitle ? flux_str_dup(info->subtitle) : NULL;
	d->show_back          = info->show_back;
	d->back_disabled      = info->back_disabled;
	d->show_pane_toggle   = info->show_pane_toggle;
	d->integrate_window   = info->integrate_window;
	d->pressed            = FLUX_TB_REGION_NONE;
	d->on_back            = info->on_back;
	d->on_back_ctx        = info->userdata;
	d->on_pane_toggle     = info->on_pane_toggle;
	d->on_pane_toggle_ctx = info->userdata;
	titlebar_set_icon(d, info->icon);
	titlebar_measure_title(d);

	nd->component_data               = d;
	nd->destroy_component_data       = titlebar_destroy;
	nd->behavior.on_pointer_down     = titlebar_pointer_down;
	nd->behavior.on_pointer_down_ctx = d;
	nd->behavior.on_click            = titlebar_click;
	nd->behavior.on_click_ctx        = d;
	nd->behavior.tooltip_at          = titlebar_tooltip_at;
	nd->behavior.tooltip_at_ctx      = d;
	nd->behavior.on_cancel           = titlebar_cancel;
	nd->behavior.on_cancel_ctx       = d;

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_CONTAINER);
	/* Fixed compact height; stretch across the container's width (a definite
	 * width would suppress the cross-axis stretch and collapse the band). */
	xent_set_height(info->ctx, node, FLUX_TB_HEIGHT);
	xent_set_flex_align_self(info->ctx, node, XENT_FLEX_ALIGN_STRETCH);
	xent_set_flex_grow(info->ctx, node, 0.0f);

	/* Window-integrated title bars remove the OS caption (and draw their own
	 * caption buttons); a normal in-content band leaves the window frame alone. */
	if (d->integrate_window && d->window) flux_window_extend_into_title_bar(d->window, true);
	return node;
}

void flux_title_bar_sync(XentContext *ctx, XentNodeId node, struct FluxNodeData *nd) {
	FluxTitleBarData *d = ( FluxTitleBarData * ) nd->component_data;
	if (!d || !d->integrate_window || !d->window) return;

	HWND hwnd   = flux_window_hwnd(d->window);
	d->maximized = hwnd && IsZoomed(hwnd);

	XentRect r = {0};
	if (!xent_get_layout_rect(ctx, node, &r) || r.w <= 0.0f) return;

	FluxRect           bounds = {r.x, r.y, r.w, r.h};
	FluxRect           local  = {0.0f, 0.0f, r.w, r.h};
	FluxTitleBarLayout l = flux_titlebar_layout(&local, d->show_back, d->show_pane_toggle, d->icon_glyph [0] != '\0', true);

	FluxRect pass [FLUX_WINDOW_MAX_TITLE_BAR_PASS];
	int      n = 0;
	if (l.has_back) pass [n++] = (FluxRect) {r.x + l.back.x, r.y + l.back.y, l.back.w, l.back.h};
	if (l.has_toggle) pass [n++] = (FluxRect) {r.x + l.toggle.x, r.y + l.toggle.y, l.toggle.w, l.toggle.h};
	pass [n++] = (FluxRect) {r.x + l.min.x, r.y + l.min.y, l.min.w, l.min.h};
	pass [n++] = (FluxRect) {r.x + l.max.x, r.y + l.max.y, l.max.w, l.max.h};
	pass [n++] = (FluxRect) {r.x + l.close.x, r.y + l.close.y, l.close.w, l.close.h};
	flux_window_set_title_bar(d->window, bounds, pass, n);
}

void flux_title_bar_set_title(FluxNodeStore *store, XentNodeId id, char const *title, char const *subtitle) {
	FluxTitleBarData *d = titlebar_data(store, id);
	if (!d) return;
	flux_str_free(d->title);
	flux_str_free(d->subtitle);
	d->title    = title ? flux_str_dup(title) : NULL;
	d->subtitle = subtitle ? flux_str_dup(subtitle) : NULL;
	titlebar_measure_title(d);
}

void flux_title_bar_set_back(FluxNodeStore *store, XentNodeId id, bool show, bool disabled) {
	FluxTitleBarData *d = titlebar_data(store, id);
	if (!d) return;
	d->show_back     = show;
	d->back_disabled = disabled;
}

void flux_title_bar_set_pane_toggle(FluxNodeStore *store, XentNodeId id, bool show) {
	FluxTitleBarData *d = titlebar_data(store, id);
	if (d) d->show_pane_toggle = show;
}
