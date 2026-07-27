#include "gallery.h"
#include "bridge/flux_internal.h"
#include "controls/data/flux_nav_view_data.h"
#include "controls/data/flux_radio_buttons_data.h"
#include "controls/data/flux_list_view_data.h"
#include "controls/data/flux_scroll_data.h"
#include "controls/data/flux_tab_view_data.h"
#include "controls/data/flux_textbox_data.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/factory/flux_init_items.h"
#include "controls/textbox/tb_internal.h"
#include "popup/flux_menu_internal.h"
#include "render/flux_engine.h"
#include "render/flux_icon.h"
#include "store/flux_node_data.h"
#include "window/flux_window_internal.h"

#include <fluxent/flux_host.h>
#include <fluxent/flux_popup.h>
#include <xtk/xtk_catalog.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

typedef struct GalleryHarness {
	Model          model;
	FluxApp       *app;
	XentCtx       *ctx;
	FluxStore     *store;
	XentNodeId     host_root;
	XtkHost       *host;
	XtkRt         *rt;
	FluxBackendCtx backend;
} GalleryHarness;

static bool show_page(GalleryHarness *g, int page);

static bool rect_valid(XentRect const *r) {
	return isfinite(r->x) && isfinite(r->y) && isfinite(r->w) && isfinite(r->h) && r->w >= 0.0f && r->h >= 0.0f;
}

static bool tree_layout_valid(GalleryHarness *g, XentNodeId node, int *count) {
	XentRect rect = {0};
	if (!xent_layout_rect(g->ctx, node, &rect) || !rect_valid(&rect)) return false;
	(*count)++;
	for (XentNodeId child = xent_node_first(g->ctx, node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(g->ctx, child))
		if (!tree_layout_valid(g, child, count)) return false;
	return true;
}

static XentNodeId find_type(GalleryHarness *g, XentNodeId node, FluxControlType type) {
	if (flux_node_type(g->store, node) == type) return node;
	for (XentNodeId child = xent_node_first(g->ctx, node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(g->ctx, child))
	{
		XentNodeId found = find_type(g, child, type);
		if (found != XENT_NODE_INVALID) return found;
	}
	return XENT_NODE_INVALID;
}

static XentNodeId find_type_n(GalleryHarness *g, XentNodeId node, FluxControlType type, int *skip) {
	if (flux_node_type(g->store, node) == type && (*skip)-- == 0) return node;
	for (XentNodeId child = xent_node_first(g->ctx, node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(g->ctx, child))
	{
		XentNodeId found = find_type_n(g, child, type, skip);
		if (found != XENT_NODE_INVALID) return found;
	}
	return XENT_NODE_INVALID;
}

static int count_type(GalleryHarness *g, XentNodeId node, FluxControlType type) {
	int count = flux_node_type(g->store, node) == type ? 1 : 0;
	for (XentNodeId child = xent_node_first(g->ctx, node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(g->ctx, child))
		count += count_type(g, child, type);
	return count;
}

static XentNodeId find_text(GalleryHarness *g, XentNodeId node, char const *text) {
	char const *content = xent_node_text(g->ctx, node);
	if (content && strcmp(content, text) == 0) return node;
	for (XentNodeId child = xent_node_first(g->ctx, node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(g->ctx, child))
	{
		XentNodeId found = find_text(g, child, text);
		if (found != XENT_NODE_INVALID) return found;
	}
	return XENT_NODE_INVALID;
}

static XentNodeId scroll_for_text(GalleryHarness *g, char const *text) {
	XentNodeId node = find_text(g, g->host_root, text);
	while (node != XENT_NODE_INVALID && flux_node_type(g->store, node) != FLUX_CONTROL_SCROLL)
		node = xent_node_parent(g->ctx, node);
	return node;
}

static XentNodeId control_for_text(GalleryHarness *g, char const *text, FluxControlType type) {
	XentNodeId node = find_text(g, g->host_root, text);
	while (node != XENT_NODE_INVALID && flux_node_type(g->store, node) != type) node = xent_node_parent(g->ctx, node);
	return node;
}

static bool window_layout(GalleryHarness *g, float width, float height) {
	FluxWindow *window = flux_app_window(g->app);
	HWND        hwnd   = flux_app_hwnd(g->app);
	if (!window || !hwnd || !window->on_render) return false;
	float scale = flux_window_dpi(window).dpi_x / 96.0f;
	if (scale <= 0.0f) return false;
	SendMessageW(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(( int ) (width * scale), ( int ) (height * scale)));
	window->on_render(window->render_ctx);
	return true;
}

static bool card_responsive(GalleryHarness *g, XentNodeId control, XentNodeId output, bool single) {
	XentNodeId items        = xent_node_parent(g->ctx, control);
	XentRect   control_rect = {0};
	XentRect   items_rect   = {0};
	XentRect   output_rect  = {0};
	if (control == XENT_NODE_INVALID
		|| output == XENT_NODE_INVALID
		|| items == XENT_NODE_INVALID
		|| !window_layout(g, 1100.0f, 760.0f))
		return false;
	if (!xent_layout_rect(g->ctx, control, &control_rect)
		|| !xent_layout_rect(g->ctx, items, &items_rect)
		|| !xent_layout_rect(g->ctx, output, &output_rect))
		return false;
	XentRect aligned        = single ? control_rect : items_rect;
	float    control_center = aligned.y + aligned.h * 0.5f;
	float    output_center  = output_rect.y + output_rect.h * 0.5f;
	if (fabsf(control_center - output_center) > 1.0f) return false;
	if (!window_layout(g, 600.0f, 760.0f)) return false;
	if (!xent_layout_rect(g->ctx, items, &items_rect) || !xent_layout_rect(g->ctx, output, &output_rect)) return false;
	bool wrapped = output_rect.y >= items_rect.y + items_rect.h;
	return window_layout(g, 1100.0f, 760.0f) && wrapped;
}

static bool button_row_wraps(GalleryHarness *g) {
	XentNodeId first      = control_for_text(g, "Accent style button", FLUX_CONTROL_BUTTON);
	XentNodeId last       = control_for_text(g, "Text style button", FLUX_CONTROL_BUTTON);
	XentRect   first_rect = {0};
	XentRect   last_rect  = {0};
	if (first == XENT_NODE_INVALID
		|| last == XENT_NODE_INVALID
		|| !window_layout(g, 1100.0f, 760.0f)
		|| !xent_layout_rect(g->ctx, first, &first_rect)
		|| !xent_layout_rect(g->ctx, last, &last_rect)
		|| fabsf(first_rect.y - last_rect.y) > 1.0f
		|| !window_layout(g, 600.0f, 760.0f)
		|| !xent_layout_rect(g->ctx, first, &first_rect)
		|| !xent_layout_rect(g->ctx, last, &last_rect))
		return false;
	bool wrapped = last_rect.y >= first_rect.y + first_rect.h;
	return window_layout(g, 1100.0f, 760.0f) && wrapped;
}

static bool route_event(GalleryHarness *g, XentNodeId node, XtkEventId id, XtkEventPayload payload) {
	XtkEvent event = {
	  .id             = id,
	  .target         = ( XtkCtrl ) node,
	  .source         = ( XtkCtrl ) node,
	  .current_target = ( XtkCtrl ) node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	  .payload        = payload,
	};
	return (flux_host_route(g->host, &event) & XTK_EVENT_HANDLED) != 0u;
}

static bool tab_layout_size(GalleryHarness *g, XentNodeId tabs, float width, float height) {
	if (!xent_layout(g->ctx, tabs, width, height)) return false;
	flux_tabs_sync_layout(g->store, tabs);
	return xent_layout(g->ctx, tabs, width, height);
}

static bool tab_items_settled(GalleryHarness *g, FluxTabViewData const *tv) {
	if (!tv->widths_valid || tv->layout_sync_pending || tv->layout_w <= 0.0f) return false;
	float width = tv->tabs [0].disp_w;
	for (int i = 0; i < tv->count; i++) {
		XentRect               tab_rect = {0};
		FluxTabViewItem const *item     = &tv->tabs [i];
		if (item->closed) continue;
		if (!xent_layout_rect(g->ctx, item->tab_node, &tab_rect)) return false;
		if (fabsf(item->disp_w - width) >= 0.5f
			|| fabsf(item->disp_w - item->cur_w) >= 0.5f
			|| fabsf(tab_rect.w - width) >= 0.5f
			|| item->w_anim)
			return false;
	}
	return true;
}

static bool tab_wide_strip_compacts(GalleryHarness *g, XentNodeId tabs, FluxTabViewData const *tv) {
	if (!tab_layout_size(g, tabs, 1000.0f, 280.0f)) return false;
	XentRect strip_rect = {0};
	XentRect host_rect  = {0};
	XentRect add_rect   = {0};
	if (!xent_layout_rect(g->ctx, tv->strip, &strip_rect)
		|| !xent_layout_rect(g->ctx, tv->tabs_host, &host_rect)
		|| !xent_layout_rect(g->ctx, tv->add_node, &add_rect))
		return false;
	float tabs_width = ( float ) tv->count * FLUX_TAB_MAX_W;
	bool  compact    = fabsf(host_rect.w - tabs_width) < 0.5f
	                && fabsf(add_rect.x - host_rect.x - host_rect.w - FLUX_TAB_ADD_PAD_L) < 0.5f
	                && strip_rect.w > host_rect.w + add_rect.w + FLUX_TAB_ADD_PAD_L;
	return compact;
}

static bool tab_narrow_strip_scrolls(GalleryHarness *g, XentNodeId tabs, FluxTabViewData const *tv) {
	if (!tab_layout_size(g, tabs, 250.0f, 280.0f)) return false;
	XentRect strip_rect = {0};
	XentRect host_rect  = {0};
	XentRect add_rect   = {0};
	if (!xent_layout_rect(g->ctx, tv->strip, &strip_rect)
		|| !xent_layout_rect(g->ctx, tv->tabs_host, &host_rect)
		|| !xent_layout_rect(g->ctx, tv->add_node, &add_rect))
		return false;
	return tv->scroll_visible && host_rect.w > 0.0f && add_rect.x + add_rect.w <= strip_rect.x + strip_rect.w + 0.5f;
}

static bool tab_widths_stable(GalleryHarness *g, XentNodeId tabs) {
	FluxNodeData    *nd = flux_store_find(g->store, tabs);
	FluxTabViewData *tv = nd ? ( FluxTabViewData * ) nd->component_data : NULL;
	if (!tv || tv->count < 2) return false;

	flux_tabs_sync_layout(g->store, tabs);
	if (!xent_layout(g->ctx, g->host_root, 1100.0f, 760.0f)
		|| !tab_items_settled(g, tv)
		|| !tab_wide_strip_compacts(g, tabs, tv)
		|| !tab_narrow_strip_scrolls(g, tabs, tv))
		return false;

	if (!xent_layout(g->ctx, g->host_root, 1100.0f, 760.0f)) return false;
	flux_tabs_sync_layout(g->store, tabs);
	if (!xent_layout(g->ctx, g->host_root, 1100.0f, 760.0f)) return false;
	return !tv->tabs [0].w_anim;
}

static bool progress_layout_valid(GalleryHarness *g) {
	XentNodeId bar       = find_type(g, g->host_root, FLUX_CONTROL_PROGRESS);
	XentNodeId ring      = find_type(g, g->host_root, FLUX_CONTROL_PROGRESS_RING);
	XentRect   bar_rect  = {0};
	XentRect   ring_rect = {0};
	if (bar == XENT_NODE_INVALID || ring == XENT_NODE_INVALID) return false;
	if (!xent_layout_rect(g->ctx, bar, &bar_rect) || !xent_layout_rect(g->ctx, ring, &ring_rect)) return false;
	return bar_rect.w > 300.0f
	    && bar_rect.h == 3.0f
	    && ring_rect.w == 32.0f
	    && ring_rect.h == 32.0f
	    && ring_rect.y >= bar_rect.y + bar_rect.h + 15.0f;
}

static bool pull_refresh_list_visible(GalleryHarness *g) {
	if (!show_page(g, PAGE_PULL_REFRESH)) return false;
	XentNodeId refresh      = find_type(g, g->host_root, FLUX_CONTROL_REFRESH);
	XentNodeId list         = find_type(g, refresh, FLUX_CONTROL_LIST);
	XentRect   refresh_rect = {0};
	XentRect   list_rect    = {0};
	if (refresh == XENT_NODE_INVALID
		|| list == XENT_NODE_INVALID
		|| !xent_layout_rect(g->ctx, refresh, &refresh_rect)
		|| !xent_layout_rect(g->ctx, list, &list_rect))
		return false;
	if (list_rect.h <= 0.0f)
		printf(
		  "pull refresh rect %.1f,%.1f %.1fx%.1f list %.1f,%.1f %.1fx%.1f\n", refresh_rect.x, refresh_rect.y,
		  refresh_rect.w, refresh_rect.h, list_rect.x, list_rect.y, list_rect.w, list_rect.h
		);
	return refresh_rect.h == 360.0f && list_rect.w > 0.0f && list_rect.h > 0.0f;
}

static bool radio_buttons_keep_row_spacing(GalleryHarness *g) {
	if (!show_page(g, PAGE_RADIO_BUTTONS)) return false;
	XentNodeId            group  = find_type(g, g->host_root, FLUX_CONTROL_RADIO_BUTTONS);
	FluxNodeData         *nd     = flux_store_find(g->store, group);
	FluxRadioButtonsData *data   = nd ? nd->component_data : NULL;
	XentRect              first  = {0};
	XentRect              second = {0};
	if (!data
		|| data->count < 2
		|| !xent_layout_rect(g->ctx, data->items [0], &first)
		|| !xent_layout_rect(g->ctx, data->items [1], &second))
		return false;
	return fabsf(second.y - first.y - first.h - 8.0f) < 0.5f;
}

static bool nav_resize_tracks_window(GalleryHarness *g) {
	XentNodeId       node = find_type(g, g->host_root, FLUX_CONTROL_NAV_VIEW);
	FluxNodeData    *nd   = flux_store_find(g->store, node);
	FluxNavViewData *nav  = nd ? nd->component_data : NULL;
	HWND             hwnd = flux_app_hwnd(g->app);
	if (!nav || !hwnd || !nav->adaptive || !nav->fill_window) return false;
	float scale = flux_window_dpi(nav->window).dpi_x / 96.0f;
	if (scale <= 0.0f) return false;

	SendMessageW(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(( int ) (600.0f * scale), ( int ) (700.0f * scale)));
	bool narrow
	  = nav->mode == FLUX_NAV_MINIMAL && fabsf(nav->width - 600.0f) < 0.5f && fabsf(nav->height - 700.0f) < 0.5f;
	if (!narrow) printf("nav narrow mode=%d width=%.1f height=%.1f\n", nav->mode, nav->width, nav->height);
	SendMessageW(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(( int ) (1100.0f * scale), ( int ) (760.0f * scale)));
	bool wide
	  = nav->mode == FLUX_NAV_EXPANDED && fabsf(nav->width - 1100.0f) < 0.5f && fabsf(nav->height - 760.0f) < 0.5f;
	if (!wide) printf("nav wide mode=%d width=%.1f height=%.1f\n", nav->mode, nav->width, nav->height);
	return narrow
	    && nav->mode == FLUX_NAV_EXPANDED
	    && fabsf(nav->width - 1100.0f) < 0.5f
	    && fabsf(nav->height - 760.0f) < 0.5f;
}

static bool harness_open(GalleryHarness *g) {
	*g = (GalleryHarness) {0};
	gallery_model_init(&g->model);
	g->app = flux_app_create(&(FluxAppCfg) {.title = L"FluXent Gallery Test", .width = 1100, .height = 760});
	if (!g->app) return false;
	ShowWindow(flux_app_hwnd(g->app), SW_HIDE);
	g->ctx       = flux_app_ctx(g->app);
	g->store     = flux_app_store(g->app);
	g->host_root = flux_app_root(g->app);
	g->host      = flux_host_adopt(g->ctx, g->store, g->host_root);
	if (!g->host) return false;
	flux_host_bind_app(g->host, g->app);
	g->backend   = (FluxBackendCtx) {.ctx = g->ctx, .store = g->store, .app = g->app, .host = g->host};
	XtkRtCfg cfg = flux_xtk_runtime_config(&g->backend);
	g->rt        = xtk_rt_create(g->host, &cfg, g->host_root, &g->model, gallery_decl_update, gallery_decl_view);
	if (!g->rt) return false;
	g->backend.runtime = g->rt;
	return true;
}

static void harness_close(GalleryHarness *g) {
	if (g->rt) xtk_rt_destroy(g->rt);
	if (g->host) flux_host_destroy(g->host);
	if (g->app) flux_app_destroy(g->app);
}

static bool show_page(GalleryHarness *g, int page) {
	g->model.page = page;
	xtk_rt_invalidate(g->rt);
	XtkFrameResult frame = xtk_rt_frame(g->rt);
	if (frame.status != XTK_FRAME_COMMITTED) {
		printf(
		  "page %d frame status %d batch error %d op %u identity %d parent %u children %d/%d path", page, frame.status,
		  frame.diagnostic.error, frame.diagnostic.failed_op, frame.identity.error, frame.identity.parent_type,
		  frame.identity.child_a, frame.identity.child_b
		);
		for (uint32_t i = 0; i < frame.identity.path_depth; i++) printf("/%d", frame.identity.path [i]);
		printf("\n");
		return false;
	}
	if (!xent_layout(g->ctx, g->host_root, 1100.0f, 760.0f)) {
		printf("page %d layout failed\n", page);
		return false;
	}
	int count = 0;
	if (!tree_layout_valid(g, g->host_root, &count)) {
		printf("page %d invalid rect\n", page);
		return false;
	}
	if (count <= 4 || count >= 4096) {
		printf("page %d node count %d\n", page, count);
		return false;
	}
	return true;
}

static bool settle(GalleryHarness *g) {
	XtkFrameResult frame = xtk_rt_frame(g->rt);
	if (frame.status != XTK_FRAME_COMMITTED && frame.status != XTK_FRAME_CLEAN) {
		printf(
		  "settle status %d batch %d op %u identity %d\n", frame.status, frame.diagnostic.error,
		  frame.diagnostic.failed_op, frame.identity.error
		);
		return false;
	}
	if (xent_layout(g->ctx, g->host_root, 1100.0f, 760.0f)) return true;
	printf("settle layout failed\n");
	return false;
}

static bool click_node(GalleryHarness *g, XentNodeId node) {
	XentRect rect = {0};
	if (!xent_layout_rect(g->ctx, node, &rect) || rect.w <= 0.0f || rect.h <= 0.0f) return false;
	float            x    = rect.x + rect.w * 0.5f;
	float            y    = rect.y + rect.h * 0.5f;
	FluxPointerEvent down = {
	  .kind           = FLUX_POINTER_DOWN,
	  .device         = FLUX_POINTER_MOUSE,
	  .x              = x,
	  .y              = y,
	  .buttons        = FLUX_POINTER_BUTTON_LEFT,
	  .changed_button = FLUX_POINTER_BUTTON_LEFT};
	FluxPointerEvent up = {
	  .kind           = FLUX_POINTER_UP,
	  .device         = FLUX_POINTER_MOUSE,
	  .x              = x,
	  .y              = y,
	  .changed_button = FLUX_POINTER_BUTTON_LEFT};
	FluxInput *input = flux_app_input(g->app);
	flux_input_dispatch(input, g->host_root, &down);
	flux_input_dispatch(input, g->host_root, &up);
	return true;
}

static void pump_messages(void) {
	MSG msg;
	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

static bool tab_width_animating(FluxTabViewData const *tv) {
	for (int i = 0; i < tv->count; i++)
		if (!tv->tabs [i].closed && tv->tabs [i].w_anim) return true;
	return false;
}

static bool finish_tab_width_animation(FluxTabViewData *tv) {
	DWORD now = GetTickCount();
	for (int i = 0; i < tv->count; i++)
		if (tv->tabs [i].w_anim) tv->tabs [i].w_start = now - 1000u;
	DWORD deadline = now + 200u;
	while (tab_width_animating(tv) && ( long ) (deadline - GetTickCount()) > 0) {
		pump_messages();
		Sleep(1);
	}
	return !tab_width_animating(tv);
}

static bool finish_tab_close(FluxTabViewData *tv, int slot) {
	tv->tabs [slot].close_start = GetTickCount() - 1000u;
	DWORD deadline              = GetTickCount() + 200u;
	while (tv->tabs [slot].closing && ( long ) (deadline - GetTickCount()) > 0) {
		pump_messages();
		Sleep(1);
	}
	return !tv->tabs [slot].closing;
}

static bool tab_close_retargets_width_animation(GalleryHarness *g) {
	int        old_count = g->model.ntab;
	XentNodeId tabs      = find_type(g, g->host_root, FLUX_CONTROL_TAB_VIEW);
	if (!route_event(g, tabs, XTK_EVENT_INVOKE, (XtkEventPayload) {0}) || !show_page(g, PAGE_TABVIEW)) return false;

	tabs                = find_type(g, g->host_root, FLUX_CONTROL_TAB_VIEW);
	FluxNodeData    *nd = flux_store_find(g->store, tabs);
	FluxTabViewData *tv = nd ? nd->component_data : NULL;
	if (!tv || !finish_tab_width_animation(tv)) return false;

	int           slot  = tv->order [0];
	FluxNodeData *close = flux_store_find(g->store, tv->tabs [slot].close_node);
	if (!close || !close->behavior.on_click) return false;
	close->behavior.on_click(close->behavior.on_click_ctx);
	if (!finish_tab_close(tv, slot)) return false;
	bool retargeted = tv->anim_active && tab_width_animating(tv);
	return retargeted && show_page(g, PAGE_TABVIEW) && g->model.ntab == old_count;
}

static bool tab_close_waits_for_strip_exit(GalleryHarness *g) {
	int        old_count = g->model.ntab;
	XentNodeId tabs      = find_type(g, g->host_root, FLUX_CONTROL_TAB_VIEW);
	if (!route_event(g, tabs, XTK_EVENT_INVOKE, (XtkEventPayload) {0}) || !show_page(g, PAGE_TABVIEW)) return false;

	tabs                = find_type(g, g->host_root, FLUX_CONTROL_TAB_VIEW);
	FluxNodeData    *nd = flux_store_find(g->store, tabs);
	FluxTabViewData *tv = nd ? nd->component_data : NULL;
	if (!tv || !finish_tab_width_animation(tv)) return false;

	int        close_slot = tv->order [0];
	int        peer_slot  = tv->order [1];
	float      peer_w     = tv->tabs [peer_slot].disp_w;
	XentNodeId close_node = tv->tabs [close_slot].close_node;
	if (!click_node(g, close_node) || !tv->tabs [close_slot].defer_width_update || !finish_tab_close(tv, close_slot))
		return false;
	if (!tv->width_update_pending
		|| !tv->width_update_deferred
		|| tab_width_animating(tv)
		|| fabsf(tv->tabs [peer_slot].disp_w - peer_w) >= 0.5f)
		return false;

	XentRect strip = {0};
	if (!xent_layout_rect(g->ctx, tv->strip, &strip)) return false;
	FluxPointerEvent move
	  = {.kind = FLUX_POINTER_MOVE, .device = FLUX_POINTER_MOUSE, .x = strip.x + 1.0f, .y = strip.y + strip.h + 1.0f};
	flux_input_dispatch(flux_app_input(g->app), g->host_root, &move);

	DWORD deadline = GetTickCount() + 200u;
	while (!tab_width_animating(tv) && ( long ) (deadline - GetTickCount()) > 0) {
		pump_messages();
		Sleep(1);
	}
	bool expanded = tab_width_animating(tv) && !tv->width_update_pending;
	return expanded && show_page(g, PAGE_TABVIEW) && g->model.ntab == old_count;
}

static int tab_open_count_for_test(FluxTabViewData const *tv) {
	int count = 0;
	for (int i = 0; i < tv->count; i++) count += !tv->tabs [i].closed && tv->tabs [i].kind == FLUX_TAB_KIND_TAB;
	return count;
}

static bool tab_view_can_empty_and_reopen(GalleryHarness *g) {
	int restore_count = g->model.ntab;
	while (g->model.ntab > 0) {
		XentNodeId tabs = find_type(g, g->host_root, FLUX_CONTROL_TAB_VIEW);
		if (!route_event(g, tabs, XTK_EVENT_CLOSE, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 0})
			|| !show_page(g, PAGE_TABVIEW))
			return false;
	}

	XentNodeId       tabs = find_type(g, g->host_root, FLUX_CONTROL_TAB_VIEW);
	FluxNodeData    *nd   = flux_store_find(g->store, tabs);
	FluxTabViewData *tv   = nd ? nd->component_data : NULL;
	if (!tv || tab_open_count_for_test(tv) != 0 || xent_node_first(g->ctx, tv->content) != XENT_NODE_INVALID)
		return false;

	while (g->model.ntab < restore_count) {
		if (!route_event(g, tabs, XTK_EVENT_INVOKE, (XtkEventPayload) {0}) || !show_page(g, PAGE_TABVIEW)) return false;
		tabs = find_type(g, g->host_root, FLUX_CONTROL_TAB_VIEW);
	}
	return g->model.ntab == restore_count;
}

static bool menu_page_records(GalleryHarness *g) {
	if (!show_page(g, PAGE_MENUS)) return false;
	XentNodeId       bar      = find_type(g, g->host_root, FLUX_CONTROL_MENU_BAR);
	FluxNodeData    *bar_nd   = flux_store_find(g->store, bar);
	FluxMenuBarData *bar_data = bar_nd ? bar_nd->component_data : NULL;
	XentRect         bar_rect = {0}, first_rect = {0}, second_rect = {0};
	if (!bar_data
		|| bar_data->count != 3
		|| !xent_layout_rect(g->ctx, bar, &bar_rect)
		|| !xent_layout_rect(g->ctx, bar_data->items [0].node, &first_rect)
		|| !xent_layout_rect(g->ctx, bar_data->items [1].node, &second_rect))
	{
		printf(
		  "menu bar runtime/layout missing data %d count %d bar %d first %d second %d\n", bar_data != NULL,
		  bar_data ? bar_data->count : -1, xent_layout_rect(g->ctx, bar, &bar_rect),
		  bar_data ? xent_layout_rect(g->ctx, bar_data->items [0].node, &first_rect) : 0,
		  bar_data ? xent_layout_rect(g->ctx, bar_data->items [1].node, &second_rect) : 0
		);
		return false;
	}
	if (fabsf(first_rect.x - bar_rect.x - FLUX_MENU_BAR_ITEM_MARGIN) >= 0.5f
		|| fabsf(second_rect.x - first_rect.x - first_rect.w - 2.0f * FLUX_MENU_BAR_ITEM_MARGIN) >= 0.5f)
	{
		printf("menu bar x %.1f first %.1f %.1f second %.1f\n", bar_rect.x, first_rect.x, first_rect.w, second_rect.x);
		return false;
	}
	FluxMenu *edit         = bar_data->items [1].flyout;
	char      expected [8] = {0};
	flux_icon_to_utf8(flux_icon_lookup("Undo"), expected, sizeof(expected));
	if (!edit
		|| edit->item_count != 2
		|| !edit->items [0].icon_glyph
		|| strcmp(edit->items [0].icon_glyph, expected) != 0)
	{
		printf(
		  "menu icon items %d glyph %s expected %s\n", edit ? edit->item_count : -1,
		  edit && edit->item_count > 0 && edit->items [0].icon_glyph ? edit->items [0].icon_glyph : "(null)", expected
		);
		return false;
	}

	FluxEngine *engine = flux_app_engine(g->app);
	flux_engine_collect(engine, g->ctx, g->host_root);

	int visible = 0;
	int hidden  = 0;
	for (uint32_t i = 0; i < flux_engine_ncmd(engine); i++) {
		FluxRenderCommand const *cmd = flux_engine_cmd(engine, i);
		if (!cmd || cmd->phase != FLUX_PHASE_MAIN || cmd->snapshot.type != FLUX_CONTROL_MENU_BAR_ITEM) continue;
		if (cmd->snapshot.u.menu.label) visible++;
		else hidden++;
		XentDl *list = flux_menuitem_record(NULL, &cmd->snapshot, &cmd->bounds, &cmd->state);
		if (!list) return false;
		xent_dl_free(list);
	}
	return visible == 3 && hidden > 0;
}

static bool repeat_button_posts_from_behavior(GalleryHarness *g) {
	if (!show_page(g, PAGE_MENUS)) return false;
	XentNodeId    repeat = find_type(g, g->host_root, FLUX_CONTROL_REPEAT_BUTTON);
	FluxNodeData *nd     = flux_store_find(g->store, repeat);
	if (!nd || !nd->behavior.on_pointer_down || !nd->behavior.on_cancel) return false;
	int before = g->model.repeats;
	nd->behavior.on_pointer_down(nd->behavior.on_pointer_down_ctx, 1.0f, 1.0f, 1);
	nd->behavior.on_cancel(nd->behavior.on_cancel_ctx);
	return settle(g) && g->model.repeats == before + 1;
}

static bool info_bar_restores_after_close(GalleryHarness *g) {
	for (int i = 0; i < BAR_COUNT; i++) g->model.bar_open [i] = true;
	if (!show_page(g, PAGE_INFOBAR)) return false;
	int count = count_type(g, g->host_root, FLUX_CONTROL_INFO_BAR);
	if (count != BAR_COUNT) {
		printf("info bar initial count %d\n", count);
		return false;
	}
	for (int closed = 0; closed < BAR_COUNT; closed++) {
		XentNodeId    bar  = find_type(g, g->host_root, FLUX_CONTROL_INFO_BAR);
		FluxNodeData *nd   = flux_store_find(g->store, bar);
		XentRect      rect = {0};
		if (!nd || !nd->behavior.on_pointer_down || !nd->behavior.on_click || !xent_layout_rect(g->ctx, bar, &rect))
			return false;
		nd->behavior.on_pointer_down(nd->behavior.on_pointer_down_ctx, rect.w - 1.0f, rect.h * 0.5f, 1);
		nd->behavior.on_click(nd->behavior.on_click_ctx);
		if (!settle(g)) return false;
		count = count_type(g, g->host_root, FLUX_CONTROL_INFO_BAR);
		if (count != BAR_COUNT - closed - 1) {
			printf(
			  "info bar count after close %d: %d, model %d%d%d%d\n", closed + 1, count, g->model.bar_open [0],
			  g->model.bar_open [1], g->model.bar_open [2], g->model.bar_open [3]
			);
			return false;
		}
	}

	XentNodeId restore = control_for_text(g, "Restore all info bars", FLUX_CONTROL_BUTTON);
	if (restore == XENT_NODE_INVALID || !click_node(g, restore) || !settle(g)) return false;
	for (int i = 0; i < BAR_COUNT; i++)
		if (!g->model.bar_open [i]) return false;
	count = count_type(g, g->host_root, FLUX_CONTROL_INFO_BAR);
	if (count != BAR_COUNT) printf("info bar restored model but count %d\n", count);
	return count == BAR_COUNT;
}

static bool list_scroll_realizes_without_selection(GalleryHarness *g) {
	g->model.list_sel = -1;
	if (!show_page(g, PAGE_LISTVIEW)) {
		printf("list page did not open\n");
		return false;
	}
	XentNodeId        list = find_type(g, g->host_root, FLUX_CONTROL_LIST);
	FluxNodeData     *nd   = flux_store_find(g->store, list);
	FluxListViewData *ld   = nd ? nd->component_data : NULL;
	XentRect          rect = {0};
	if (!ld || !xent_layout_rect(g->ctx, ld->scroll, &rect)) {
		printf("list root or scroll layout missing\n");
		return false;
	}
	FluxNodeData   *scroll_nd   = flux_store_find(g->store, ld->scroll);
	FluxScrollData *scroll_data = scroll_nd ? scroll_nd->component_data : NULL;
	if (!scroll_data || !ld->on_stale || !scroll_data->on_offset_changed) {
		printf("list stale binding missing\n");
		return false;
	}
	int              old_first = ld->realized_first;
	FluxPointerEvent wheel     = {
	  .kind     = FLUX_POINTER_WHEEL,
	  .device   = FLUX_POINTER_MOUSE,
	  .x        = rect.x + rect.w * 0.5f,
	  .y        = rect.y + rect.h * 0.5f,
	  .wheel_dy = -30.0f};
	flux_input_dispatch(flux_app_input(g->app), g->host_root, &wheel);
	if (scroll_data->scroll_y <= 0.0f) {
		printf("list wheel did not move inner scroll\n");
		return false;
	}
	if (!settle(g)) return false;
	nd = flux_store_find(g->store, list);
	if (!nd) {
		XentNodeId next = find_type(g, g->host_root, FLUX_CONTROL_LIST);
		printf("list recreated %llu -> %llu\n", ( unsigned long long ) list, ( unsigned long long ) next);
		nd = flux_store_find(g->store, next);
	}
	ld = nd ? nd->component_data : NULL;
	if (!ld || old_first != 0 || ld->realized_first <= 0)
		printf(
		  "list window old %d new %d..%d after scroll %.1f selection %d\n", old_first, ld ? ld->realized_first : -999,
		  ld ? ld->realized_last : -999, scroll_data->scroll_y, g->model.list_sel
		);
	return ld && old_first == 0 && ld->realized_first > 0 && g->model.list_sel < 0;
}

static bool navigate(GalleryHarness *g, int page) {
	XentNodeId       nav  = find_type(g, g->host_root, FLUX_CONTROL_NAV_VIEW);
	FluxNodeData    *nd   = flux_store_find(g->store, nav);
	FluxNavViewData *data = nd ? nd->component_data : NULL;
	if (!data || page < 0 || page >= data->count) return false;
	flux_input_route_activation(g->host, g->store, data->items [page].node, XTK_EVENT_CAUSE_USER);
	return settle(g) && g->model.page == page;
}

static bool nav_transition_keeps_children_coherent(GalleryHarness *g) {
	if (!navigate(g, PAGE_CAT_BASIC)) {
		printf("nav transition could not select Basic input\n");
		return false;
	}
	XentNodeId       node = find_type(g, g->host_root, FLUX_CONTROL_NAV_VIEW);
	FluxNodeData    *nd   = flux_store_find(g->store, node);
	FluxNavViewData *nav  = nd ? nd->component_data : NULL;
	if (!nav) {
		printf("nav transition missing runtime\n");
		return false;
	}

	FluxNavViewItem *basic = &nav->items [PAGE_CAT_BASIC];
	if (!basic->expanded || basic->children_host == XENT_NODE_INVALID) {
		printf("nav transition Basic input is not expanded\n");
		return false;
	}
	flux_nav_setmode(g->store, node, FLUX_NAV_PANE_LEFT_COMPACT);
	bool closing        = nav->mode == FLUX_NAV_COMPACT
	                   && nav->labels_visible
	                   && xent_node_parent(g->ctx, basic->children_host) == nav->items_host;
	nav->pane_w.current = FLUX_NAV_PANE_COMPACT;
	nav->pane_w.active  = false;
	flux_nav_setmode(g->store, node, FLUX_NAV_PANE_LEFT_COMPACT);
	bool compact = !nav->labels_visible && xent_node_parent(g->ctx, basic->children_host) == XENT_NODE_INVALID;

	flux_nav_setmode(g->store, node, FLUX_NAV_PANE_LEFT);
	bool opening        = nav->mode == FLUX_NAV_EXPANDED
	                   && !nav->labels_visible
	                   && xent_node_parent(g->ctx, basic->children_host) == XENT_NODE_INVALID;
	nav->pane_w.current = FLUX_NAV_PANE_EXPANDED;
	nav->pane_w.active  = false;
	flux_nav_setmode(g->store, node, FLUX_NAV_PANE_LEFT);
	bool expanded = nav->labels_visible && xent_node_parent(g->ctx, basic->children_host) == nav->items_host;
	flux_nav_setmode(g->store, node, FLUX_NAV_PANE_AUTO);
	if (!closing || !compact || !opening || !expanded)
		printf(
		  "nav transition closing=%d compact=%d opening=%d expanded=%d mode=%d pane=%.1f labels=%d parent=%llu\n",
		  closing, compact, opening, expanded, nav->mode, nav->pane_w.current, nav->labels_visible,
		  ( unsigned long long ) xent_node_parent(g->ctx, basic->children_host)
		);
	return closing && compact && opening && expanded;
}

static bool page_scroll_resets(GalleryHarness *g) {
	if (!navigate(g, PAGE_LISTVIEW)) return false;
	XentNodeId      old_scroll = scroll_for_text(g, "ListView");
	FluxNodeData   *old_nd     = flux_store_find(g->store, old_scroll);
	FluxScrollData *old_data   = old_nd ? old_nd->component_data : NULL;
	if (!old_data) return false;
	old_data->scroll_y = 500.0f;

	if (!navigate(g, PAGE_BUTTON)) return false;
	XentNodeId      next_scroll = scroll_for_text(g, "Button");
	FluxNodeData   *next_nd     = flux_store_find(g->store, next_scroll);
	FluxScrollData *next_data   = next_nd ? next_nd->component_data : NULL;
	return next_data && next_scroll != old_scroll && next_data->scroll_y == 0.0f;
}

static bool page_scroll_survives_actions(GalleryHarness *g) {
	if (!navigate(g, PAGE_CAT_BASIC) || !navigate(g, PAGE_BUTTON)) return false;
	XentNodeId       nav      = find_type(g, g->host_root, FLUX_CONTROL_NAV_VIEW);
	FluxNodeData    *nav_nd   = flux_store_find(g->store, nav);
	FluxNavViewData *nav_data = nav_nd ? nav_nd->component_data : NULL;
	XentNodeId       scroll   = scroll_for_text(g, "Button");
	XentNodeId       button   = control_for_text(g, "Standard button", FLUX_CONTROL_BUTTON);
	FluxNodeData    *nd       = flux_store_find(g->store, scroll);
	FluxScrollData  *data     = nd ? nd->component_data : NULL;
	if (!nav_data || !data || button == XENT_NODE_INVALID) return false;
	data->scroll_y = 100.0f;

	for (int i = 0; i < 14; i++) {
		if (!route_event(g, button, XTK_EVENT_INVOKE, (XtkEventPayload) {0}) || !settle(g)) return false;
		nd   = flux_store_find(g->store, scroll);
		data = nd ? nd->component_data : NULL;
		if (g->model.page != PAGE_BUTTON || nav_data->selected != PAGE_BUTTON || !data || data->scroll_y != 100.0f) {
			printf(
			  "action %d page %d nav %d scroll %.1f\n", i + 1, g->model.page, nav_data->selected,
			  data ? data->scroll_y : -1.0f
			);
			return false;
		}
	}
	return true;
}

static bool page_scroll_survives_pointer_actions(GalleryHarness *g) {
	if (!navigate(g, PAGE_CAT_BASIC) || !navigate(g, PAGE_BUTTON)) return false;
	XentNodeId       nav         = find_type(g, g->host_root, FLUX_CONTROL_NAV_VIEW);
	FluxNodeData    *nav_nd      = flux_store_find(g->store, nav);
	FluxNavViewData *nav_data    = nav_nd ? nav_nd->component_data : NULL;

	XentNodeId       scroll      = scroll_for_text(g, "Button");
	FluxNodeData    *scroll_nd   = flux_store_find(g->store, scroll);
	FluxScrollData  *scroll_data = scroll_nd ? scroll_nd->component_data : NULL;
	if (!scroll_data) {
		printf("pointer sequence found no button page scroll\n");
		return false;
	}
	scroll_data->scroll_y = 100.0f;

	for (int i = 0; i < 14; i++) {
		XentNodeId checkbox = control_for_text(g, "Disable button", FLUX_CONTROL_CHECKBOX);
		if (checkbox == XENT_NODE_INVALID || !click_node(g, checkbox) || !settle(g)) {
			printf("pointer sequence failed checkbox action %d\n", i + 1);
			return false;
		}
		nav         = find_type(g, g->host_root, FLUX_CONTROL_NAV_VIEW);
		nav_nd      = flux_store_find(g->store, nav);
		nav_data    = nav_nd ? nav_nd->component_data : NULL;
		scroll_nd   = flux_store_find(g->store, scroll);
		scroll_data = scroll_nd ? scroll_nd->component_data : NULL;
		if (g->model.page != PAGE_BUTTON
			|| !nav_data
			|| nav_data->selected != PAGE_BUTTON
			|| !scroll_data
			|| scroll_data->scroll_y != 100.0f)
		{
			printf(
			  "pointer action %d page %d nav %d scroll %.1f\n", i + 1, g->model.page,
			  nav_data ? nav_data->selected : -1, scroll_data ? scroll_data->scroll_y : -1.0f
			);
			return false;
		}
	}
	return true;
}

static bool popup_and_overlay_cleanup(GalleryHarness *g) {
	if (!navigate(g, PAGE_COMBO)) return false;
	XentNodeId    combo    = find_type(g, g->host_root, FLUX_CONTROL_COMBO_BOX);
	FluxNodeData *combo_nd = flux_store_find(g->store, combo);
	if (!combo_nd || !combo_nd->behavior.on_click) return false;
	combo_nd->behavior.on_click(combo_nd->behavior.on_click_ctx);
	if (!navigate(g, PAGE_BUTTON) || xent_node_valid(g->ctx, combo)) return false;
	if (flux_popup_lightdismiss_visible(flux_app_hwnd(g->app))) return false;

	if (!navigate(g, PAGE_DIALOG)) return false;
	XentNodeId open = control_for_text(g, "Show dialog", FLUX_CONTROL_BUTTON);
	if (!route_event(g, open, XTK_EVENT_INVOKE, (XtkEventPayload) {0}) || !settle(g)) return false;
	XentNodeId overlay = flux_app_overlay(g->app);
	if (find_type(g, overlay, FLUX_CONTROL_CONTENT_DIALOG) == XENT_NODE_INVALID) return false;
	if (!navigate(g, PAGE_BUTTON)) return false;
	return find_type(g, overlay, FLUX_CONTROL_CONTENT_DIALOG) == XENT_NODE_INVALID;
}

static bool input_state_survives_recreation(GalleryHarness *g) {
	if (!navigate(g, PAGE_TEXTBOX)) return false;
	XentNodeId           textbox  = find_type(g, g->host_root, FLUX_CONTROL_TEXT_INPUT);
	XentNodeId           password = find_type(g, g->host_root, FLUX_CONTROL_PASSWORD_BOX);
	FluxNodeData        *text_nd  = flux_store_find(g->store, textbox);
	FluxNodeData        *pass_nd  = flux_store_find(g->store, password);
	FluxTextBoxData     *text     = text_nd ? text_nd->component_data : NULL;
	FluxPasswordBoxData *pass     = pass_nd ? pass_nd->component_data : NULL;
	if (!text || !pass || strcmp(text->content, "declarative") || strcmp(pass->content, "secret")) return false;

	if (!navigate(g, PAGE_AUTOSUGGEST)) return false;
	textbox = find_type(g, g->host_root, FLUX_CONTROL_TEXT_INPUT);
	text_nd = flux_store_find(g->store, textbox);
	text    = text_nd ? text_nd->component_data : NULL;
	return text && !strcmp(text->content, "Am");
}

static bool input_behaviors_update_model(GalleryHarness *g) {
	g->model.combo_sel = 0;
	if (!show_page(g, PAGE_COMBO)) return false;
	XentNodeId    combo = find_type(g, g->host_root, FLUX_CONTROL_COMBO_BOX);
	FluxNodeData *nd    = flux_store_find(g->store, combo);
	if (!nd
		|| !nd->behavior.on_key
		|| !nd->behavior.on_key(nd->behavior.on_key_ctx, FLUX_KEY_DOWN, true)
		|| !settle(g)
		|| g->model.combo_sel != 1
		|| find_text(g, g->host_root, "Selected: Green") == XENT_NODE_INVALID)
		return false;

	g->model.rating_value = 2.0;
	if (!show_page(g, PAGE_RATING)) return false;
	XentNodeId rating = find_type(g, g->host_root, FLUX_CONTROL_RATING);
	nd                = flux_store_find(g->store, rating);
	if (!nd
		|| !nd->behavior.on_key
		|| !nd->behavior.on_key(nd->behavior.on_key_ctx, FLUX_KEY_RIGHT, true)
		|| !settle(g)
		|| g->model.rating_value != 3.0
		|| find_text(g, g->host_root, "Value: 3") == XENT_NODE_INVALID)
		return false;

	if (!show_page(g, PAGE_TEXTBOX)) return false;
	XentNodeId textbox = find_type(g, g->host_root, FLUX_CONTROL_TEXT_INPUT);
	nd                 = flux_store_find(g->store, textbox);
	if (!nd || !nd->behavior.on_char) return false;
	nd->behavior.on_char(nd->behavior.on_char_ctx, L'Z');
	if (!settle(g) || !g->model.name || !strchr(g->model.name, 'Z')) return false;
	char text_output [128];
	snprintf(text_output, sizeof(text_output), "Hello, %s!", g->model.name);
	if (find_text(g, g->host_root, text_output) == XENT_NODE_INVALID) return false;

	if (!show_page(g, PAGE_NUMBERBOX)) return false;
	XentNodeId number                 = find_type(g, g->host_root, FLUX_CONTROL_NUMBER_BOX);
	nd                                = flux_store_find(g->store, number);
	FluxTextBoxInputData *number_data = nd ? nd->component_data : NULL;
	if (!number_data) return false;
	nb_set_value(number_data, 42.0);
	if (!settle(g) || g->model.quantity != 42.0 || find_text(g, g->host_root, "Quantity: 42") == XENT_NODE_INVALID)
		return false;

	if (!show_page(g, PAGE_AUTOSUGGEST)) return false;
	textbox = find_type(g, g->host_root, FLUX_CONTROL_TEXT_INPUT);
	nd      = flux_store_find(g->store, textbox);
	if (!nd || !nd->behavior.on_char) return false;
	nd->behavior.on_char(nd->behavior.on_char_ctx, L'Z');
	if (!settle(g) || !g->model.asb_text || !strchr(g->model.asb_text, 'Z')) return false;
	XentNodeId suggest = find_type(g, g->host_root, FLUX_CONTROL_AUTO_SUGGEST);
	XentNodeId query   = find_type(g, suggest, FLUX_CONTROL_BUTTON);
	nd                 = flux_store_find(g->store, query);
	if (!nd || !nd->behavior.on_click) return false;
	nd->behavior.on_click(nd->behavior.on_click_ctx);
	if (!settle(g) || !g->model.asb_query) return false;
	char query_output [128];
	snprintf(query_output, sizeof(query_output), "Submitted: %s", g->model.asb_query);
	return find_text(g, g->host_root, query_output) != XENT_NODE_INVALID;
}

static bool dialog_has_only_configured_buttons(GalleryHarness *g) {
	if (!show_page(g, PAGE_DIALOG)) return false;
	XentNodeId open = control_for_text(g, "Show dialog", FLUX_CONTROL_BUTTON);
	if (!route_event(g, open, XTK_EVENT_INVOKE, (XtkEventPayload) {0}) || !settle(g)) return false;
	XentNodeId dialog = find_type(g, flux_app_overlay(g->app), FLUX_CONTROL_CONTENT_DIALOG);
	return dialog != XENT_NODE_INVALID && count_type(g, dialog, FLUX_CONTROL_BUTTON) == 2;
}

static bool teaching_tip_close_updates_model(GalleryHarness *g) {
	g->model.tip_open = true;
	if (!show_page(g, PAGE_TEACHING_TIP)) return false;
	XentNodeId tip = find_type(g, g->host_root, FLUX_CONTROL_TEACHING_TIP);
	if (tip == XENT_NODE_INVALID) return false;
	flux_teachtip_hide(g->store, tip);

	DWORD deadline = GetTickCount() + 500u;
	while (flux_teachtip_is_open(g->store, tip) && ( long ) (deadline - GetTickCount()) > 0) {
		pump_messages();
		Sleep(1);
	}
	if (flux_teachtip_is_open(g->store, tip))
		printf(
		  "TeachingTip close timeout runtime=%d reason=%d\n", flux_teachtip_is_open(g->store, tip),
		  g->model.tip_close_reason
		);
	return !flux_teachtip_is_open(g->store, tip) && settle(g) && !g->model.tip_open;
}

static bool tree_expansion_is_model_driven(GalleryHarness *g) {
	if (!navigate(g, PAGE_TREE_VIEW)) return false;
	XentNodeId tree     = find_type(g, g->host_root, FLUX_CONTROL_TREE_VIEW);
	int        selected = flux_tree_flat_node(g->store, tree, g->model.tree_sel);
	if (!flux_tree_is_selected(g->store, tree, selected)) return false;
	XtkEventPayload payload = {
	  .kind         = XTK_EVENT_PAYLOAD_INDEX_BOOL,
	  .u.index_bool = {.index = 3, .value = true},
	};
	if (!route_event(g, tree, XTK_EVENT_TREE_EXPAND, payload) || !settle(g)) return false;
	selected = flux_tree_flat_node(g->store, tree, g->model.tree_sel);
	if (!g->model.tree_pictures_expanded
		|| !flux_tree_is_expanded(g->store, tree, 3)
		|| !flux_tree_is_selected(g->store, tree, selected))
		return false;
	if (!navigate(g, PAGE_BUTTON) || !navigate(g, PAGE_TREE_VIEW)) return false;
	tree     = find_type(g, g->host_root, FLUX_CONTROL_TREE_VIEW);
	selected = flux_tree_flat_node(g->store, tree, g->model.tree_sel);
	return flux_tree_is_expanded(g->store, tree, 3) && flux_tree_is_selected(g->store, tree, selected);
}

typedef struct InteractionCase {
	int             page;
	FluxControlType type;
	int             skip;
	XtkEventId      event;
	XtkEventPayload payload;
	bool            replaced;
} InteractionCase;

#define INTERACT(page_, type_, event_, ...) {page_, type_, 0, event_, __VA_ARGS__, false}

static bool check_interaction(GalleryHarness *g, InteractionCase const *test) {
	if (!show_page(g, test->page)) return false;
	int        skip = test->skip;
	XentNodeId node = find_type_n(g, g->host_root, test->type, &skip);
	if (node == XENT_NODE_INVALID) return false;
	Model before = g->model;
	if (!route_event(g, node, test->event, test->payload) || !settle(g)) return false;
	if (!memcmp(&before, &g->model, sizeof(before))) return false;
	return test->replaced || xent_node_valid(g->ctx, node);
}

static bool check_interactions(GalleryHarness *g) {
	InteractionCase const cases [] = {
	  INTERACT(PAGE_BUTTON, FLUX_CONTROL_BUTTON, XTK_EVENT_INVOKE, (XtkEventPayload) {0}
          ),
	  INTERACT(PAGE_SPLIT, FLUX_CONTROL_SPLIT_BUTTON, XTK_EVENT_INVOKE, (XtkEventPayload) {0}
          ),
	  INTERACT(
		PAGE_CHECKBOX, FLUX_CONTROL_CHECKBOX, XTK_EVENT_CHANGE_BOOL,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = true}
	  ),
	  {PAGE_RADIO, FLUX_CONTROL_RADIO, 1, XTK_EVENT_CHANGE_BOOL, {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = true}, false},
	  INTERACT(
		PAGE_COMBO, FLUX_CONTROL_COMBO_BOX, XTK_EVENT_SELECT,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(
		PAGE_SLIDER, FLUX_CONTROL_SLIDER, XTK_EVENT_CHANGE_F32,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_F32, .u.f32 = 37.0f}
	  ),
	  INTERACT(
		PAGE_TOGGLE, FLUX_CONTROL_SWITCH, XTK_EVENT_CHANGE_BOOL,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = true}
	  ),
	  INTERACT(
		PAGE_RADIO_BUTTONS, FLUX_CONTROL_RADIO_BUTTONS, XTK_EVENT_SELECT,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(
		PAGE_RATING, FLUX_CONTROL_RATING, XTK_EVENT_RATING_CHANGED,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_RATING, .u.rating = {.value = 3.0, .count = 5}}
	  ),
	  INTERACT(
		PAGE_TOGGLE_SPLIT, FLUX_CONTROL_TOGGLE_SPLIT_BUTTON, XTK_EVENT_CHANGE_BOOL,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = true}
	  ),
	  INTERACT(
		PAGE_TEXTBOX, FLUX_CONTROL_TEXT_INPUT, XTK_EVENT_CHANGE_STRING,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_STRING, .u.string = {.data = "declarative", .len = 11}}
	  ),
	  INTERACT(
		PAGE_TEXTBOX, FLUX_CONTROL_PASSWORD_BOX, XTK_EVENT_CHANGE_STRING,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_STRING, .u.string = {.data = "secret", .len = 6}}
	  ),
	  INTERACT(
		PAGE_AUTOSUGGEST, FLUX_CONTROL_AUTO_SUGGEST, XTK_EVENT_CHANGE_STRING,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_STRING, .u.string = {.data = "Am", .len = 2}}
	  ),
	  INTERACT(
		PAGE_NUMBERBOX, FLUX_CONTROL_NUMBER_BOX, XTK_EVENT_CHANGE_F64,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_F64, .u.f64 = 12.0}
	  ),
	  {PAGE_INFOBAR, FLUX_CONTROL_INFO_BAR, 0, XTK_EVENT_CLOSE, {0}, true},
	  {PAGE_BADGE, FLUX_CONTROL_BUTTON, 0, XTK_EVENT_INVOKE, {0}, false},
	  INTERACT(
		PAGE_PROGRESS, FLUX_CONTROL_SLIDER, XTK_EVENT_CHANGE_F32,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_F32, .u.f32 = 37.0f}
	  ),
	  INTERACT(
		PAGE_FLIPVIEW, FLUX_CONTROL_FLIP_VIEW, XTK_EVENT_SELECT,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(
		PAGE_PAGER, FLUX_CONTROL_PAGER, XTK_EVENT_SELECT, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(PAGE_MENUS, FLUX_CONTROL_REPEAT_BUTTON, XTK_EVENT_INVOKE, (XtkEventPayload) {0}
          ),
	  INTERACT(
		PAGE_EXPANDER, FLUX_CONTROL_EXPANDER, XTK_EVENT_TOGGLE,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = true}
	  ),
	  INTERACT(
		PAGE_BREADCRUMB, FLUX_CONTROL_BREADCRUMB_BAR, XTK_EVENT_SELECT,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(
		PAGE_SELECTOR_BAR, FLUX_CONTROL_SELECTOR_BAR, XTK_EVENT_SELECT,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(
		PAGE_LISTVIEW, FLUX_CONTROL_LIST, XTK_EVENT_SELECT,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(
		PAGE_GRIDVIEW, FLUX_CONTROL_GRID_VIEW, XTK_EVENT_SELECT,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(
		PAGE_LISTBOX, FLUX_CONTROL_LIST_BOX, XTK_EVENT_SELECT,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(
		PAGE_TREE_VIEW, FLUX_CONTROL_TREE_VIEW, XTK_EVENT_TREE_SELECT,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_INDEX_BOOL, .u.index_bool = {.index = 1, .value = true}}
	  ),
	  INTERACT(
		PAGE_ITEMS_VIEW, FLUX_CONTROL_ITEMS_VIEW, XTK_EVENT_INVOKE,
		(XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}
	  ),
	  INTERACT(PAGE_PULL_REFRESH, FLUX_CONTROL_REFRESH, XTK_EVENT_REFRESH, (XtkEventPayload) {0}
          ),
	  INTERACT(PAGE_SPLIT_VIEW, FLUX_CONTROL_BUTTON, XTK_EVENT_INVOKE, (XtkEventPayload) {0}
          ),
	  INTERACT(PAGE_TITLE_BAR, FLUX_CONTROL_TITLE_BAR, XTK_EVENT_INVOKE, (XtkEventPayload) {0}
          ),
	  INTERACT(PAGE_DIALOG, FLUX_CONTROL_BUTTON, XTK_EVENT_INVOKE, (XtkEventPayload) {0}
          ),
	  INTERACT(PAGE_TEACHING_TIP, FLUX_CONTROL_BUTTON, XTK_EVENT_INVOKE, (XtkEventPayload) {0}
          ),
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases [0]); i++)
		if (!check_interaction(g, &cases [i])) {
			printf("interaction failed at case %zu page %d\n", i, cases [i].page);
			return false;
		}
	return true;
}

int main(void) {
	GalleryHarness g;
	EXPECT(harness_open(&g), "declarative gallery harness");

	for (int page = 0; page < PAGE_COUNT; page++) {
		if (show_page(&g, page)) continue;
		printf("FAIL: declarative page %d mounts and lays out\n", page);
		harness_close(&g);
		return 1;
	}
	XentNodeId       nav_root = find_type(&g, g.host_root, FLUX_CONTROL_NAV_VIEW);
	FluxNodeData    *nav_nd   = flux_store_find(g.store, nav_root);
	FluxNavViewData *nav_data = nav_nd ? nav_nd->component_data : NULL;
	EXPECT(nav_data && nav_data->adaptive && nav_data->fill_window, "declarative NavigationView tracks window size");
	EXPECT(nav_resize_tracks_window(&g), "declarative NavigationView adapts across window thresholds");
	EXPECT(
	  nav_transition_keeps_children_coherent(&g),
	  "NavigationView children follow the presented pane width during adaptive transitions"
	);
	for (int page = 0; page < PAGE_COUNT; page++) {
		if (page == PAGE_SEP_1) continue;
		EXPECT(navigate(&g, page), "real NavigationView event selects every page");
	}
	EXPECT(page_scroll_resets(&g), "page navigation resets the declarative scroll viewport");
	EXPECT(page_scroll_survives_actions(&g), "page scroll survives repeated declarative actions");
	EXPECT(page_scroll_survives_pointer_actions(&g), "page scroll survives repeated declarative pointer actions");
	EXPECT(popup_and_overlay_cleanup(&g), "page replacement destroys popups and overlay controls");
	EXPECT(check_interactions(&g), "declarative controls route, reconcile, and retain identity");
	EXPECT(g.model.pass_chars == 6, "PasswordBox string payload reaches the shared model");
	EXPECT(input_state_survives_recreation(&g), "controlled input state survives declarative page recreation");
	EXPECT(input_behaviors_update_model(&g), "declarative input behavior reaches the shared model");
	EXPECT(
	  tree_expansion_is_model_driven(&g), "tree expansion survives declarative reconciliation and page recreation"
	);
	EXPECT(menu_page_records(&g), "menu page snapshots distinguish bar visuals from flyout data");
	EXPECT(repeat_button_posts_from_behavior(&g), "declarative repeat behavior posts its first invoke");
	EXPECT(info_bar_restores_after_close(&g), "declarative info bars restore after close");
	EXPECT(dialog_has_only_configured_buttons(&g), "content dialog omits its empty secondary action");
	EXPECT(teaching_tip_close_updates_model(&g), "TeachingTip close updates declarative state");
	EXPECT(list_scroll_realizes_without_selection(&g), "declarative list scrolling realizes rows without selection");
	EXPECT(show_page(&g, PAGE_PROGRESS) && progress_layout_valid(&g), "progress controls keep WinUI geometry");
	EXPECT(pull_refresh_list_visible(&g), "pull-to-refresh list occupies its viewport");
	EXPECT(radio_buttons_keep_row_spacing(&g), "declarative RadioButtons keep WinUI row spacing");
	EXPECT(
	  show_page(&g, PAGE_TEXTBOX)
		&& card_responsive(
		  &g, find_type(&g, g.host_root, FLUX_CONTROL_TEXT_INPUT),
		  xent_node_parent(g.ctx, find_text(&g, g.host_root, "Output:")), true
		),
	  "textbox card aligns wide content and wraps narrow output"
	);
	EXPECT(
	  show_page(&g, PAGE_BUTTON)
		&& card_responsive(
		  &g, control_for_text(&g, "Standard button", FLUX_CONTROL_BUTTON),
		  xent_node_parent(g.ctx, find_text(&g, g.host_root, "Output:")), false
		),
	  "button card keeps its control group intact while wrapping"
	);
	EXPECT(show_page(&g, PAGE_BUTTON) && button_row_wraps(&g), "button style row wraps only at narrow width");

	static int const stress [] = {
	  PAGE_TABVIEW,
	  PAGE_MENUS,
	  PAGE_EXPANDER,
	  PAGE_TABVIEW,
	  PAGE_EXPANDER,
	  PAGE_MENUS,
	  PAGE_TABVIEW,
	};
	for (int round = 0; round < 16; round++)
		for (size_t i = 0; i < sizeof(stress) / sizeof(stress [0]); i++)
			EXPECT(show_page(&g, stress [i]), "page-switch sequence remains stable");

	EXPECT(show_page(&g, PAGE_TABVIEW), "tab view page");
	XentNodeId tabs = find_type(&g, g.host_root, FLUX_CONTROL_TAB_VIEW);
	EXPECT(tabs != XENT_NODE_INVALID, "tab view retained node");
	FluxNodeData    *tabs_nd   = flux_store_find(g.store, tabs);
	FluxTabViewData *tabs_data = tabs_nd ? tabs_nd->component_data : NULL;
	XentRect         add_rect  = {0};
	EXPECT(
	  tabs_data
		&& tabs_data->add_visible
		&& tabs_data->add_node != XENT_NODE_INVALID
		&& xent_layout_rect(g.ctx, tabs_data->add_node, &add_rect)
		&& add_rect.w > 0.0f
		&& add_rect.h > 0.0f,
	  "declarative tab add button is visible"
	);
	EXPECT(tab_widths_stable(&g, tabs), "tab widths settle before paint");
	EXPECT(
	  route_event(&g, tabs, XTK_EVENT_SELECT, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 1}),
	  "tab selection event"
	);
	EXPECT(show_page(&g, PAGE_TABVIEW) && g.model.tab_sel == 1, "tab selection reconciles");
	tabs                 = find_type(&g, g.host_root, FLUX_CONTROL_TAB_VIEW);
	tabs_nd              = flux_store_find(g.store, tabs);
	tabs_data            = tabs_nd ? tabs_nd->component_data : NULL;
	XentNodeId first_tab = tabs_data ? tabs_data->tabs [tabs_data->order [0]].tab_node : XENT_NODE_INVALID;
	int        old_count = g.model.ntab;
	EXPECT(route_event(&g, tabs, XTK_EVENT_INVOKE, (XtkEventPayload) {0}), "tab add event");
	EXPECT(show_page(&g, PAGE_TABVIEW) && g.model.ntab == old_count + 1, "tab add reconciles");
	tabs           = find_type(&g, g.host_root, FLUX_CONTROL_TAB_VIEW);
	tabs_nd        = flux_store_find(g.store, tabs);
	tabs_data      = tabs_nd ? tabs_nd->component_data : NULL;
	int added_slot = tabs_data ? tabs_data->order [old_count] : -1;
	EXPECT(
	  tabs_data
		&& first_tab != XENT_NODE_INVALID
		&& tabs_data->tabs [tabs_data->order [0]].tab_node == first_tab
		&& added_slot >= 0
		&& tabs_data->tabs [added_slot].w_anim,
	  "tab add retains existing tabs and starts its transition"
	);
	FluxNodeData *close_nd
	  = tabs_data ? flux_store_find(g.store, tabs_data->tabs [tabs_data->order [0]].close_node) : NULL;
	EXPECT(close_nd && close_nd->behavior.on_click, "tab close button behavior");
	close_nd->behavior.on_click(close_nd->behavior.on_click_ctx);
	EXPECT(
	  tabs_data->tabs [tabs_data->order [0]].closing && g.model.ntab == old_count + 1,
	  "tab close waits for its transition before changing the model"
	);
	EXPECT(show_page(&g, PAGE_BUTTON) && show_page(&g, PAGE_TABVIEW), "tab close transition can be abandoned safely");
	tabs = find_type(&g, g.host_root, FLUX_CONTROL_TAB_VIEW);
	EXPECT(
	  route_event(&g, tabs, XTK_EVENT_CLOSE, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = 0}),
	  "tab close event"
	);
	EXPECT(show_page(&g, PAGE_TABVIEW) && g.model.ntab == old_count, "tab close reconciles");
	EXPECT(tab_close_retargets_width_animation(&g), "tab close keeps the retargeted width animation scheduled");
	EXPECT(tab_close_waits_for_strip_exit(&g), "pointer close keeps peer widths until the pointer leaves the strip");
	EXPECT(tab_view_can_empty_and_reopen(&g), "TabView clears its content at zero tabs and can add again");

	harness_close(&g);
	printf("PASS: declarative gallery page switching / layout\n");
	return 0;
}
