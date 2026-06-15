#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_str.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_flyout.h"
#include "fluxent/flux_menu_flyout.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>

/* SplitButton routes a pointer-down by zone: the left primary zone fires the
 * action callback; the right secondary zone (last 36px) toggles the flyout. The
 * binding is allocated at create time so the primary action works before any
 * flyout is attached, and flux_split_button_set_flyout_ex fills in the popup. */
typedef struct FluxSplitButtonBinding {
	FluxFlyout     *flyout;
	FluxMenuFlyout *menu; /**< WinUI SplitButton typically hosts a MenuFlyout; if set it wins. */
	FluxPlacement   placement;
	FluxNodeStore  *store;
	XentNodeId      node;
	FluxWindow     *window;
	XentContext    *ctx;
	bool            pressed_secondary; /**< Which zone the active press landed in. */
} FluxSplitButtonBinding;

static bool split_secondary_zone(FluxSplitButtonBinding const *b, float local_x) {
	XentRect r = {0};
	xent_get_layout_rect(b->ctx, b->node, &r);
	return local_x >= r.w - (FLUX_SPLIT_SECONDARY_W + FLUX_SPLIT_DIVIDER_W);
}

static void split_invoke_primary(FluxSplitButtonBinding const *b) {
	FluxNodeData   *nd = flux_node_store_get(b->store, b->node);
	FluxButtonData *bd = nd ? ( FluxButtonData * ) nd->component_data : NULL;
	if (bd && bd->on_click) bd->on_click(bd->on_click_ctx);
}

/* Open the secondary flyout. A hosted MenuFlyout opens with the right input kind (so a
 * keyboard invoke highlights the first item) and is dismissed via light-dismiss / Esc;
 * a generic flyout toggles. */
static void split_toggle_flyout(FluxSplitButtonBinding const *b, FluxMenuInputKind kind) {
	if (b->menu) {
		if (!flux_menu_flyout_is_visible(b->menu)) {
			FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
			flux_menu_flyout_show_for_input(b->menu, anchor, FLUX_PLACEMENT_BOTTOM, kind);
		}
		return;
	}
	if (!b->flyout) return;
	if (flux_flyout_is_visible(b->flyout)) {
		flux_flyout_dismiss(b->flyout);
		return;
	}
	FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
	flux_flyout_show(b->flyout, anchor, b->placement);
}

/* WinUI SplitButton is two inner Buttons that fire on Click (release). Record which zone
 * the press captured; the action runs on release-within-bounds (split_on_click), giving
 * press-cancel for free. An open flyout is light-dismissed by the outside press first. */
static void split_on_pointer_down(void *ctx, float local_x, float local_y, int click_count) {
	( void ) local_y;
	( void ) click_count;
	FluxSplitButtonBinding *b = ( FluxSplitButtonBinding * ) ctx;
	if (b) b->pressed_secondary = split_secondary_zone(b, local_x);
}

static void split_on_click(void *ctx) {
	FluxSplitButtonBinding *b = ( FluxSplitButtonBinding * ) ctx;
	if (!b) return;
	if (b->pressed_secondary) split_toggle_flyout(b, FLUX_MENU_INPUT_MOUSE);
	else split_invoke_primary(b);
}

/* Keyboard parity with WinUI SplitButton: Space/Enter invokes the primary action;
 * Alt+Down or F4 opens the flyout; Escape dismisses it. */
static bool split_on_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxSplitButtonBinding *b = ( FluxSplitButtonBinding * ) ctx;
	if (!b) return false;

	if (vk == VK_RETURN || vk == VK_SPACE) {
		split_invoke_primary(b);
		return true;
	}
	bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
	if ((vk == VK_DOWN && alt) || vk == VK_F4) {
		split_toggle_flyout(b, FLUX_MENU_INPUT_KEYBOARD);
		return true;
	}
	/* A hosted MenuFlyout handles Escape via the active-menu key routing; only the
	 * generic flyout needs an explicit Escape-to-dismiss here. */
	if (vk == VK_ESCAPE && !b->menu && b->flyout && flux_flyout_is_visible(b->flyout)) {
		flux_flyout_dismiss(b->flyout);
		return true;
	}
	return false;
}

XentNodeId flux_create_split_button(FluxSplitButtonCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SPLIT_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData           *nd = flux_node_store_get(info->store, node);
	FluxButtonData         *bd = nd ? ( FluxButtonData * ) calloc(1, sizeof(FluxButtonData)) : NULL;
	FluxSplitButtonBinding *b  = nd ? ( FluxSplitButtonBinding * ) calloc(1, sizeof(*b)) : NULL;
	if (!nd || !bd || !b) {
		free(bd);
		free(b);
		return node;
	}
	bd->label                        = flux_str_dup(info->label);
	bd->icon_name                    = flux_str_dup(info->icon_name);
	bd->font_size                    = 14.0f;
	bd->style                        = FLUX_BUTTON_STANDARD;
	bd->on_click                     = info->on_click;
	bd->on_click_ctx                 = info->userdata;

	b->placement                     = FLUX_PLACEMENT_BOTTOM;
	b->store                         = info->store;
	b->node                          = node;
	b->ctx                           = info->ctx;

	nd->component_data               = bd;
	nd->destroy_component_data       = flux_button_data_destroy;
	nd->behavior.on_pointer_down     = split_on_pointer_down;
	nd->behavior.on_pointer_down_ctx = b;
	nd->behavior.on_click            = split_on_click;
	nd->behavior.on_click_ctx        = b;
	nd->behavior.on_key              = split_on_key;
	nd->behavior.on_key_ctx          = b;

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);
	if (info->label) xent_set_semantic_label(info->ctx, node, info->label);
	xent_set_focusable(info->ctx, node, true);

	/* Right inset reserves the divider + secondary chevron zone after the label. */
	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  info->ctx, node, info->label, {11.0f, 5.0f, 11.0f + FLUX_SPLIT_DIVIDER_W + FLUX_SPLIT_SECONDARY_W, 6.0f},
	  {0.0f, 32.0f},
         {68.0f, 32.0f}
    });

	return node;
}

void flux_split_button_set_flyout_ex(FluxFlyoutBindingInfo const *info) {
	if (!info || !info->store || !info->flyout) return;
	FluxNodeData *nd = flux_node_store_get(info->store, info->id);
	if (!nd || nd->behavior.on_pointer_down != split_on_pointer_down) return;

	FluxSplitButtonBinding *b = ( FluxSplitButtonBinding * ) nd->behavior.on_pointer_down_ctx;
	if (!b) return;
	b->flyout    = info->flyout;
	b->placement = info->placement;
	b->window    = info->window;
	if (info->xctx) b->ctx = info->xctx;
}

void flux_split_button_set_menu_flyout_ex(FluxContextFlyoutBindingInfo const *info) {
	if (!info || !info->store || !info->menu) return;
	FluxNodeData *nd = flux_node_store_get(info->store, info->id);
	if (!nd || nd->behavior.on_pointer_down != split_on_pointer_down) return;

	FluxSplitButtonBinding *b = ( FluxSplitButtonBinding * ) nd->behavior.on_pointer_down_ctx;
	if (!b) return;
	b->menu   = info->menu;
	b->window = info->window;
	if (info->xctx) b->ctx = info->xctx;
}
