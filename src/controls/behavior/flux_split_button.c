#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_str.h"

#include "controls/factory/flux_init_basic.h"
#include "fluxent/flux_flyout.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_menu.h"
#include <xtk/xtk_catalog.h>

#include <math.h>
#include <stdlib.h>
#include <windows.h>

typedef struct FluxSplitButtonBinding {
	FluxFlyout   *flyout;
	FluxMenu     *menu;
	FluxPlacement placement;
	FluxStore    *store;
	XentNodeId    node;
	FluxWindow   *window;
	XentCtx      *ctx;
	bool          pressed_secondary;
	bool          toggle;
} FluxSplitButtonBinding;

typedef struct FluxSplitButtonState {
	FluxButtonData         button;
	FluxSplitButtonBinding binding;
} FluxSplitButtonState;

static bool split_secondary_zone(FluxSplitButtonBinding const *b, float local_x) {
	XentRect r = {0};
	xent_layout_rect(b->ctx, b->node, &r);
	return local_x >= r.w - (FLUX_SPLIT_SECONDARY_W + FLUX_SPLIT_DIVIDER_W);
}

static void split_route(FluxSplitButtonBinding const *b, XtkEventId id, XtkEventPayload payload) {
	XtkHost *host = flux_store_host(b->store);
	if (!host) return;
	XtkEvent event = {
	  .id             = id,
	  .target         = ( XtkCtrl ) b->node,
	  .source         = ( XtkCtrl ) b->node,
	  .current_target = ( XtkCtrl ) b->node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	  .payload        = payload,
	};
	flux_host_route(host, &event);
}

static void split_invoke_primary(FluxSplitButtonBinding const *b) {
	FluxNodeData   *nd = flux_store_find(b->store, b->node);
	FluxButtonData *bd = nd ? ( FluxButtonData * ) nd->component_data : NULL;
	if (!bd) return;
	if (b->toggle) {
		bd->is_checked                = !bd->is_checked;
		bool  checked                 = bd->is_checked;
		void  (*toggle)(void *, bool) = bd->on_toggle;
		void  (*click)(void *)        = bd->on_click;
		void *toggle_ctx              = bd->on_toggle_ctx;
		void *click_ctx               = bd->on_click_ctx;
		if (toggle) toggle(toggle_ctx, checked);
		if (click) click(click_ctx);
		split_route(b, XTK_EVENT_CHANGE_BOOL, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = checked});
		split_route(b, XTK_EVENT_INVOKE, (XtkEventPayload) {0});
		return;
	}
	if (bd->on_click) bd->on_click(bd->on_click_ctx);
	split_route(b, XTK_EVENT_INVOKE, (XtkEventPayload) {0});
}

static void split_toggle_flyout(FluxSplitButtonBinding const *b, FluxMenuInputKind kind) {
	if (b->menu) {
		if (!flux_menu_visible(b->menu)) {
			FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
			flux_menu_showinput(b->menu, anchor, FLUX_PLACEMENT_BOTTOM, kind);
		}
		return;
	}
	if (!b->flyout) return;
	if (flux_flyout_visible(b->flyout)) {
		flux_flyout_dismiss(b->flyout);
		return;
	}
	FluxRect anchor = flux_binding_screen_anchor(b->window, b->ctx, b->store, b->node);
	flux_flyout_show(b->flyout, anchor, b->placement);
}

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

static bool split_on_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxSplitButtonBinding *b = ( FluxSplitButtonBinding * ) ctx;
	if (!b) return false;

	if (key == FLUX_KEY_ENTER || key == FLUX_KEY_SPACE) {
		split_invoke_primary(b);
		return true;
	}
	bool alt = flux_key_held(FLUX_KEY_ALT);
	if ((key == FLUX_KEY_DOWN && alt) || key == FLUX_KEY_F4) {
		split_toggle_flyout(b, FLUX_MENU_INPUT_KEYBOARD);
		return true;
	}
	if (key == FLUX_KEY_ESCAPE && !b->menu && b->flyout && flux_flyout_visible(b->flyout)) {
		flux_flyout_dismiss(b->flyout);
		return true;
	}
	return false;
}

XentNodeId flux_split_init(FluxSplitButtonDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SPLIT_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData         *nd = flux_store_find(info->store, node);
	FluxSplitButtonState *s  = nd ? ( FluxSplitButtonState * ) calloc(1, sizeof(*s)) : NULL;
	if (!nd || !s) {
		free(s);
		return node;
	}
	FluxButtonData         *bd       = &s->button;
	FluxSplitButtonBinding *b        = &s->binding;
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

	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_BUTTON);
	if (info->label) xent_sem_setlabel(info->ctx, node, info->label);
	xent_set_focusable(info->ctx, node, true);

	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  info->ctx, node, info->label, {11.0f, 5.0f, 11.0f + FLUX_SPLIT_DIVIDER_W + FLUX_SPLIT_SECONDARY_W, 6.0f},
	  {0.0f, 32.0f},
         {68.0f, 32.0f}
    });

	return node;
}

XentNodeId flux_togglesplit_init(FluxToggleSplitButtonDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_TOGGLE_SPLIT_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData         *nd = flux_store_find(info->store, node);
	FluxSplitButtonState *s  = nd ? ( FluxSplitButtonState * ) calloc(1, sizeof(*s)) : NULL;
	if (!nd || !s) {
		free(s);
		return node;
	}
	FluxButtonData         *bd       = &s->button;
	FluxSplitButtonBinding *b        = &s->binding;
	bd->label                        = flux_str_dup(info->label);
	bd->icon_name                    = flux_str_dup(info->icon_name);
	bd->font_size                    = 14.0f;
	bd->style                        = FLUX_BUTTON_STANDARD;
	bd->is_checked                   = info->checked;
	bd->on_toggle                    = info->on_toggle;
	bd->on_toggle_ctx                = info->on_toggle_ctx;
	bd->on_click                     = info->on_click;
	bd->on_click_ctx                 = info->on_click_ctx;

	b->placement                     = FLUX_PLACEMENT_BOTTOM;
	b->store                         = info->store;
	b->node                          = node;
	b->ctx                           = info->ctx;
	b->toggle                        = true;

	nd->component_data               = bd;
	nd->destroy_component_data       = flux_button_data_destroy;
	nd->behavior.on_pointer_down     = split_on_pointer_down;
	nd->behavior.on_pointer_down_ctx = b;
	nd->behavior.on_click            = split_on_click;
	nd->behavior.on_click_ctx        = b;
	nd->behavior.on_key              = split_on_key;
	nd->behavior.on_key_ctx          = b;

	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_BUTTON);
	if (info->label) xent_sem_setlabel(info->ctx, node, info->label);
	xent_set_focusable(info->ctx, node, true);

	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  info->ctx, node, info->label, {11.0f, 5.0f, 11.0f + FLUX_SPLIT_DIVIDER_W + FLUX_SPLIT_SECONDARY_W, 6.0f},
	  {0.0f, 32.0f},
         {68.0f, 32.0f}
    });

	return node;
}

void flux_togglesplit_set_checked(FluxStore *store, XentNodeId id, bool checked) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (nd && nd->component_type == FLUX_CONTROL_TOGGLE_SPLIT_BUTTON && nd->component_data)
		(( FluxButtonData * ) nd->component_data)->is_checked = checked;
}

void flux_split_retarget(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || !nd->behavior.on_pointer_down_ctx) return;
	FluxSplitButtonBinding *b = ( FluxSplitButtonBinding * ) nd->behavior.on_pointer_down_ctx;
	b->store                  = store;
	b->node                   = id;
	b->ctx                    = flux_store_ctx(store);
}

void flux_prop_split_menu_ex(FluxContextFlyoutBindingInfo const *info) {
	if (!info || !info->store || !info->menu) return;
	FluxNodeData *nd = flux_store_find(info->store, info->id);
	if (!nd || nd->behavior.on_pointer_down != split_on_pointer_down) return;

	FluxSplitButtonBinding *b = ( FluxSplitButtonBinding * ) nd->behavior.on_pointer_down_ctx;
	if (!b) return;
	b->menu   = info->menu;
	b->window = info->window;
	if (info->xctx) b->ctx = info->xctx;
}

void flux_split_clear_menu(FluxStore *store, XentNodeId id, FluxMenu *menu) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->behavior.on_pointer_down != split_on_pointer_down) return;
	FluxSplitButtonBinding *b = ( FluxSplitButtonBinding * ) nd->behavior.on_pointer_down_ctx;
	if (b && b->menu == menu) b->menu = NULL;
}
