#include "controls/factory/flux_factory.h"

#include "fluxent/flux_flyout.h"
#include "fluxent/flux_window.h"

#include <stdlib.h>
#include <windows.h>

static void compute_scroll_offset(XentContext *ctx, FluxNodeStore *store, XentNodeId node, float *out_x, float *out_y) {
	*out_x       = 0.0f;
	*out_y       = 0.0f;
	XentNodeId p = xent_get_parent(ctx, node);
	while (p != XENT_NODE_INVALID) {
		if (xent_get_control_type(ctx, p) != XENT_CONTROL_SCROLL) {
			p = xent_get_parent(ctx, p);
			continue;
		}

		FluxNodeData   *pnd = flux_node_store_get(store, p);
		FluxScrollData *sd  = pnd ? ( FluxScrollData * ) pnd->component_data : NULL;
		if (sd) {
			*out_x += sd->scroll_x;
			*out_y += sd->scroll_y;
		}
		p = xent_get_parent(ctx, p);
	}
}

static FluxRect anchor_for_binding(FluxWindow *window, XentContext *ctx, FluxNodeStore *store, XentNodeId node) {
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
	float    scale = dpi > 0 ? ( float ) dpi / 96.0f : 1.0f;

	XentRect lr    = {0};
	xent_get_layout_rect(ctx, node, &lr);

	float scroll_x = 0.0f;
	float scroll_y = 0.0f;
	compute_scroll_offset(ctx, store, node, &scroll_x, &scroll_y);

	POINT pt = {( LONG ) ((lr.x - scroll_x) * scale), ( LONG ) ((lr.y - scroll_y) * scale)};
	ClientToScreen(hwnd, &pt);

	anchor.x = ( float ) pt.x;
	anchor.y = ( float ) pt.y;
	anchor.w = lr.width * scale;
	anchor.h = lr.height * scale;
	return anchor;
}

typedef struct FluxFlyoutBinding {
	FluxFlyout    *flyout;
	FluxPlacement  placement;
	FluxNodeStore *store;
	XentNodeId     node;
	FluxWindow    *window;
	XentContext   *ctx;
} FluxFlyoutBinding;

static void flyout_on_pointer_down(void *ctx, float local_x, float local_y, int click_count) {
	( void ) click_count;
	( void ) local_x;
	( void ) local_y;
	FluxFlyoutBinding *b = ( FluxFlyoutBinding * ) ctx;
	if (!b || !b->flyout) return;

	if (flux_flyout_is_visible(b->flyout)) {
		flux_flyout_dismiss(b->flyout);
		return;
	}

	FluxRect anchor = anchor_for_binding(b->window, b->ctx, b->store, b->node);
	flux_flyout_show(b->flyout, anchor, b->placement);
}

void flux_node_set_flyout(FluxNodeStore *store, XentNodeId id, FluxFlyout *flyout, FluxPlacement placement) {
	FluxFlyoutBindingInfo info = {store, id, flyout, placement, NULL, NULL};
	flux_node_set_flyout_ex(&info);
}

void flux_node_set_flyout_ex(FluxFlyoutBindingInfo const *info) {
	if (!info || !info->store || !info->flyout) return;
	FluxNodeData *nd = flux_node_store_get(info->store, info->id);
	if (!nd) return;

	FluxFlyoutBinding *b = ( FluxFlyoutBinding * ) calloc(1, sizeof(*b));
	if (!b) return;

	b->flyout                        = info->flyout;
	b->placement                     = info->placement;
	b->store                         = info->store;
	b->node                          = info->id;
	b->ctx                           = info->xctx;
	b->window                        = info->window;

	nd->behavior.on_pointer_down     = flyout_on_pointer_down;
	nd->behavior.on_pointer_down_ctx = b;
}

typedef struct FluxContextMenuBinding {
	FluxMenuFlyout *menu;
	FluxNodeStore  *store;
	XentNodeId      node;
	FluxWindow     *window;
	XentContext    *ctx;
} FluxContextMenuBinding;

static void context_menu_on_context_binding(void *ctx, float local_x, float local_y) {
	( void ) local_x;
	( void ) local_y;
	FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) ctx;
	if (!b || !b->menu) return;

	FluxRect anchor = anchor_for_binding(b->window, b->ctx, b->store, b->node);
	flux_menu_flyout_show(b->menu, anchor, FLUX_PLACEMENT_BOTTOM);
}

void flux_node_set_context_flyout(FluxNodeStore *store, XentNodeId id, FluxMenuFlyout *menu) {
	FluxContextFlyoutBindingInfo info = {store, id, menu, NULL, NULL};
	flux_node_set_context_flyout_ex(&info);
}

void flux_node_set_context_flyout_ex(FluxContextFlyoutBindingInfo const *info) {
	if (!info || !info->store || !info->menu) return;
	FluxNodeData *nd = flux_node_store_get(info->store, info->id);
	if (!nd) return;

	FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) calloc(1, sizeof(*b));
	if (!b) return;

	b->menu                          = info->menu;
	b->store                         = info->store;
	b->node                          = info->id;
	b->ctx                           = info->xctx;
	b->window                        = info->window;

	nd->behavior.on_context_menu     = context_menu_on_context_binding;
	nd->behavior.on_context_menu_ctx = b;
}
