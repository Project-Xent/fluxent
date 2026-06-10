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
		if (flux_get_control_type(ctx, p) != FLUX_CONTROL_SCROLL) {
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

FluxRect flux_binding_screen_anchor(FluxWindow *window, XentContext *ctx, FluxNodeStore *store, XentNodeId node) {
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
	xent_get_layout_rect(ctx, node, &lr);

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
	FluxFlyout    *flyout;
	FluxPlacement  placement;
	FluxNodeStore *store;
	XentNodeId     node;
	FluxWindow    *window;
	XentContext   *ctx;
} FluxFlyoutBinding;

/* WinUI Button.Flyout opens on Click (pointer release), not press -- Button::OnClick ->
 * OpenAssociatedFlyout. An already-open flyout is light-dismissed by the outside press
 * before this runs, so the click path only ever has to open. */
static void flyout_on_click(void *ctx) {
	FluxFlyoutBinding *b = ( FluxFlyoutBinding * ) ctx;
	if (!b || !b->flyout || flux_flyout_is_visible(b->flyout)) return;

	FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
	flux_flyout_show(b->flyout, anchor, b->placement);
}

/* Keyboard parity with WinUI DropDownButton (a Button hosting a Flyout): invoking the
 * button with Space/Enter opens the flyout; Escape light-dismisses it. */
static bool flyout_on_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxFlyoutBinding *b = ( FluxFlyoutBinding * ) ctx;
	if (!b || !b->flyout) return false;

	bool visible = flux_flyout_is_visible(b->flyout);
	if (!visible && (vk == VK_RETURN || vk == VK_SPACE)) {
		FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
		flux_flyout_show(b->flyout, anchor, b->placement);
		return true;
	}
	if (visible && vk == VK_ESCAPE) {
		flux_flyout_dismiss(b->flyout);
		return true;
	}
	return false;
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

	b->flyout                 = info->flyout;
	b->placement              = info->placement;
	b->store                  = info->store;
	b->node                   = info->id;
	b->ctx                    = info->xctx;
	b->window                 = info->window;

	nd->behavior.on_click     = flyout_on_click;
	nd->behavior.on_click_ctx = b;
	nd->behavior.on_key       = flyout_on_key;
	nd->behavior.on_key_ctx   = b;
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

	FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
	flux_menu_flyout_show(b->menu, anchor, FLUX_PLACEMENT_BOTTOM);
}

void flux_node_set_context_flyout(FluxNodeStore *store, XentNodeId id, FluxMenuFlyout *menu) {
	FluxContextFlyoutBindingInfo info = {store, id, menu, NULL, NULL};
	flux_node_set_context_flyout_ex(&info);
}

/* Primary-invoke MenuFlyout (WinUI Button.Flyout == MenuFlyout): the host opens the
 * menu on click or Space/Enter. A keyboard invoke opens it with FLUX_MENU_INPUT_KEYBOARD
 * so the first item is highlighted immediately; pointer opens it without a highlight. */
static void menu_flyout_binding_show(FluxContextMenuBinding const *b, FluxMenuInputKind kind) {
	if (flux_menu_flyout_is_visible(b->menu)) {
		flux_menu_flyout_dismiss(b->menu);
		return;
	}
	FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
	flux_menu_flyout_show_for_input(b->menu, anchor, FLUX_PLACEMENT_BOTTOM, kind);
}

static void menu_flyout_on_click(void *ctx) {
	FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) ctx;
	if (b && b->menu && !flux_menu_flyout_is_visible(b->menu)) menu_flyout_binding_show(b, FLUX_MENU_INPUT_MOUSE);
}

static bool menu_flyout_on_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) ctx;
	if (!b || !b->menu) return false;
	if (vk != VK_RETURN && vk != VK_SPACE && vk != VK_DOWN) return false;
	if (!flux_menu_flyout_is_visible(b->menu)) menu_flyout_binding_show(b, FLUX_MENU_INPUT_KEYBOARD);
	return true;
}

void flux_node_set_menu_flyout(FluxNodeStore *store, XentNodeId id, FluxMenuFlyout *menu) {
	FluxContextFlyoutBindingInfo info = {store, id, menu, NULL, NULL};
	flux_node_set_menu_flyout_ex(&info);
}

void flux_node_set_menu_flyout_ex(FluxContextFlyoutBindingInfo const *info) {
	if (!info || !info->store || !info->menu) return;
	FluxNodeData *nd = flux_node_store_get(info->store, info->id);
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
