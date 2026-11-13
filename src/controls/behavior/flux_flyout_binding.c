#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"

#include "fluxent/flux_flyout.h"
#include "fluxent/flux_window.h"

#include <stdlib.h>
#include <windows.h>

static void compute_scroll_offset(XentCtx *ctx, FluxStore *store, XentNodeId node, float *out_x, float *out_y) {
	*out_x       = 0.0f;
	*out_y       = 0.0f;
	XentNodeId p = xent_node_parent(ctx, node);
	while (p != XENT_NODE_INVALID) {
		if (flux_node_type(store, p) != FLUX_CONTROL_SCROLL) {
			p = xent_node_parent(ctx, p);
			continue;
		}

		FluxNodeData   *pnd = flux_store_find(store, p);
		FluxScrollData *sd  = pnd ? ( FluxScrollData * ) pnd->component_data : NULL;
		if (sd) {
			*out_x += flux_scroll_off_x(sd);
			*out_y += flux_scroll_off_y(sd);
		}
		p = xent_node_parent(ctx, p);
	}
}

FluxRect flux_binding_window_rect(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	XentRect lr = {0};
	float    sx = 0.0f, sy = 0.0f;
	if (!ctx || !xent_layout_rect(ctx, node, &lr)) return (FluxRect) {0};
	compute_scroll_offset(ctx, store, node, &sx, &sy);
	return (FluxRect) {lr.x - sx, lr.y - sy, lr.w, lr.h};
}

FluxRect flux_binding_screen_anchor(FluxWindow *window, XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxRect anchor = {0.0f, 0.0f, 0.0f, 0.0f};
	if (!window || !ctx || node == XENT_NODE_INVALID) {
		POINT cursor;
		GetCursorPos(&cursor);
		anchor.x = ( float ) cursor.x;
		anchor.y = ( float ) cursor.y;
		return anchor;
	}

	HWND     hwnd  = flux_window_hwnd(window);
	UINT     dpi   = GetDpiForWindow(hwnd);
	float    scale = dpi > 0 ? ( float ) dpi / FLUX_DPI_BASE : 1.0f;

	XentRect lr    = {0};
	xent_layout_rect(ctx, node, &lr);

	float scroll_x = 0.0f;
	float scroll_y = 0.0f;
	compute_scroll_offset(ctx, store, node, &scroll_x, &scroll_y);

	POINT pt = {( LONG ) ((lr.x - scroll_x) * scale), ( LONG ) ((lr.y - scroll_y) * scale)};
	ClientToScreen(hwnd, &pt);

	anchor.x = ( float ) pt.x;
	anchor.y = ( float ) pt.y;
	anchor.w = lr.w * scale;
	anchor.h = lr.h * scale;
	return anchor;
}

typedef struct FluxFlyoutBinding {
	FluxFlyout   *flyout;
	FluxPlacement placement;
	FluxStore    *store;
	XentNodeId    node;
	FluxWindow   *window;
	XentCtx      *ctx;
} FluxFlyoutBinding;

typedef struct FluxContextMenuBinding {
	FluxMenu   *menu;
	FluxStore  *store;
	XentNodeId  node;
	FluxWindow *window;
	XentCtx    *ctx;
} FluxContextMenuBinding;

static void menu_flyout_binding_show(FluxContextMenuBinding const *b, FluxMenuInputKind kind) {
	if (flux_menu_visible(b->menu)) {
		flux_menu_dismiss(b->menu);
		return;
	}
	FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
	flux_menu_showinput(b->menu, anchor, FLUX_PLACEMENT_BOTTOM, kind);
}

static void menu_flyout_on_click(void *ctx) {
	FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) ctx;
	if (b && b->menu && !flux_menu_visible(b->menu)) menu_flyout_binding_show(b, FLUX_MENU_INPUT_MOUSE);
}

static bool menu_flyout_on_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) ctx;
	if (!b || !b->menu) return false;
	if (key != FLUX_KEY_ENTER && key != FLUX_KEY_SPACE && key != FLUX_KEY_DOWN) return false;
	if (!flux_menu_visible(b->menu)) menu_flyout_binding_show(b, FLUX_MENU_INPUT_KEYBOARD);
	return true;
}

void flux_prop_menu_ex(FluxContextFlyoutBindingInfo const *info) {
	if (!info || !info->store || !info->menu) return;
	FluxNodeData *nd = flux_store_find(info->store, info->id);
	if (!nd) return;

	FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) calloc(1, sizeof(*b));
	if (!b) return;

	b->menu                   = info->menu;
	b->store                  = info->store;
	b->node                   = info->id;
	b->ctx                    = info->xctx;
	b->window                 = info->window;

	nd->behavior.on_click     = menu_flyout_on_click;
	nd->behavior.on_click_ctx = b;
	nd->behavior.on_key       = menu_flyout_on_key;
	nd->behavior.on_key_ctx   = b;
}
