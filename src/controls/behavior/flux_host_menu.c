#include "controls/behavior/flux_host_menu.h"

#include "bridge/flux_internal.h"
#include "controls/behavior/flux_flyout_binding.h"
#include "controls/data/flux_menu_bar_data.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_nav.h"
#include "fluxent/flux_menu.h"
#include "render/flux_icon.h"
#include "runtime/flux_str.h"
#include "store/flux_node_data.h"

#include <stdlib.h>
#include <windows.h>

typedef struct FluxHostMenuFlyData {
	XentCtx    *ctx;
	FluxStore  *store;
	FluxWindow *window;
	FluxTheme  *theme;
	FluxText   *text;
	XentNodeId  node;
	XentNodeId  parent;
	char const *title;
	FluxMenu   *menu;
} FluxHostMenuFlyData;

typedef struct FluxHostMenuItemData {
	FluxMenuItemVisual visual;
	FluxStore         *store;
	XentNodeId         node;
	XentNodeId         parent;
	char const        *label;
	char const        *icon;
	bool               enabled;
} FluxHostMenuItemData;

static FluxHostMenuFlyData  *menu_fly_data(FluxStore *store, XentNodeId node);
static FluxHostMenuItemData *menu_item_data(FluxStore *store, XentNodeId node);

static void                  menu_item_invoke(void *ctx) {
	FluxHostMenuItemData *item = ( FluxHostMenuItemData * ) ctx;
	XtkHost              *host = item ? flux_store_host(item->store) : NULL;
	if (host) flux_input_route_activation(host, item->store, item->node, XTK_EVENT_CAUSE_USER);
}

static void menu_fly_show(FluxHostMenuFlyData *fly, FluxMenuInputKind input) {
	if (!fly || !fly->menu || flux_menu_visible(fly->menu)) return;
	FluxRect anchor = flux_binding_screen_anchor(fly->window, fly->ctx, fly->store, fly->parent);
	flux_menu_showinput(fly->menu, anchor, FLUX_PLACEMENT_BOTTOM, input);
}

static void menu_fly_click(void *ctx) { menu_fly_show(( FluxHostMenuFlyData * ) ctx, FLUX_MENU_INPUT_MOUSE); }

static bool menu_fly_key(void *ctx, FluxKey key, bool down) {
	if (!down || (key != FLUX_KEY_ENTER && key != FLUX_KEY_SPACE && key != FLUX_KEY_DOWN)) return false;
	menu_fly_show(( FluxHostMenuFlyData * ) ctx, FLUX_MENU_INPUT_KEYBOARD);
	return true;
}

static void menu_fly_unbind(FluxHostMenuFlyData *fly, XentNodeId parent) {
	FluxNodeData *nd = flux_store_find(fly->store, parent);
	if (!nd) return;
	if (nd->component_type == FLUX_CONTROL_DROPDOWN_BUTTON) {
		if (nd->behavior.on_click_ctx == fly) {
			nd->behavior.on_click     = NULL;
			nd->behavior.on_click_ctx = NULL;
		}
		if (nd->behavior.on_key_ctx == fly) {
			nd->behavior.on_key     = NULL;
			nd->behavior.on_key_ctx = NULL;
		}
		return;
	}
	if (nd->component_type == FLUX_CONTROL_SPLIT_BUTTON || nd->component_type == FLUX_CONTROL_TOGGLE_SPLIT_BUTTON)
		flux_split_clear_menu(fly->store, parent, fly->menu);
	else if (nd->component_type == FLUX_CONTROL_MENU_BAR) flux_host_menu_sync_bar(fly->store, parent);
}

static void menu_fly_bind(FluxHostMenuFlyData *fly, XentNodeId parent) {
	FluxNodeData *nd = flux_store_find(fly->store, parent);
	if (!nd) return;
	if (nd->component_type == FLUX_CONTROL_DROPDOWN_BUTTON) {
		nd->behavior.on_click     = menu_fly_click;
		nd->behavior.on_click_ctx = fly;
		nd->behavior.on_key       = menu_fly_key;
		nd->behavior.on_key_ctx   = fly;
		return;
	}
	if (nd->component_type == FLUX_CONTROL_SPLIT_BUTTON || nd->component_type == FLUX_CONTROL_TOGGLE_SPLIT_BUTTON) {
		flux_prop_split_menu_ex(&(FluxContextFlyoutBindingInfo) {
		  .store  = fly->store,
		  .id     = parent,
		  .menu   = fly->menu,
		  .xctx   = fly->ctx,
		  .window = fly->window,
		});
	}
}

static void menu_fly_destroy(void *component_data) {
	FluxHostMenuFlyData *fly = ( FluxHostMenuFlyData * ) component_data;
	if (!fly) return;
	menu_fly_unbind(fly, fly->parent);
	if (fly->menu) flux_menu_destroy(fly->menu);
	flux_str_free(fly->title);
	free(fly);
}

static void menu_item_destroy(void *component_data) {
	FluxHostMenuItemData *item = ( FluxHostMenuItemData * ) component_data;
	if (!item) return;
	if (item->parent != XENT_NODE_INVALID) flux_host_menu_sync_fly(item->store, item->parent);
	flux_str_free(item->label);
	flux_str_free(item->icon);
	free(item);
}

static FluxHostMenuFlyData *menu_fly_data(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || nd->destroy_component_data != menu_fly_destroy) return NULL;
	return ( FluxHostMenuFlyData * ) nd->component_data;
}

static FluxHostMenuItemData *menu_item_data(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || nd->destroy_component_data != menu_item_destroy) return NULL;
	return ( FluxHostMenuItemData * ) nd->component_data;
}

XentNodeId flux_host_menufly_parent(FluxStore *store, XentNodeId node) {
	XentCtx *ctx = flux_store_ctx(store);
	if (!ctx || !xent_node_valid(ctx, node)) return XENT_NODE_INVALID;
	XentNodeId parent = xent_node_parent(ctx, node);
	if (parent == XENT_NODE_INVALID) return parent;
	XentNodeId owner = xent_node_parent(ctx, parent);
	if (owner != XENT_NODE_INVALID && flux_menubar_content_node(store, owner) == parent) return owner;
	return parent;
}

bool flux_host_menufly_build(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxControlEnv const *env) {
	FluxNodeData        *nd  = flux_store_find(store, node);
	FluxHostMenuFlyData *fly = nd ? ( FluxHostMenuFlyData * ) calloc(1, sizeof(*fly)) : NULL;
	if (!nd || !fly) {
		free(fly);
		return false;
	}
	fly->ctx    = ctx;
	fly->store  = store;
	fly->window = env ? env->window : NULL;
	fly->theme  = env ? env->theme : NULL;
	fly->text   = env ? env->text : NULL;
	fly->node   = node;
	fly->parent = XENT_NODE_INVALID;
	fly->menu   = flux_menu_create(fly->window);
	if (fly->menu) {
		flux_menu_setthememanager(fly->menu, fly->theme);
		flux_menu_settext(fly->menu, fly->text);
	}
	nd->component_data         = fly;
	nd->destroy_component_data = menu_fly_destroy;
	xent_setsize(ctx, node, (XentSize) {0.0f, 0.0f});
	return true;
}

bool flux_host_menuitem_build(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxNodeData         *nd   = flux_store_find(store, node);
	FluxHostMenuItemData *item = nd ? ( FluxHostMenuItemData * ) calloc(1, sizeof(*item)) : NULL;
	if (!nd || !item) {
		free(item);
		return false;
	}
	item->store                = store;
	item->node                 = node;
	item->parent               = XENT_NODE_INVALID;
	item->enabled              = true;
	nd->component_data         = item;
	nd->destroy_component_data = menu_item_destroy;
	xent_setsize(ctx, node, (XentSize) {0.0f, 0.0f});
	return true;
}

void flux_host_menufly_set_text(FluxStore *store, XentNodeId node, char const *text) {
	FluxHostMenuFlyData *fly = menu_fly_data(store, node);
	if (fly) flux_str_replace(&fly->title, text);
}

void flux_host_menuitem_set_text(FluxStore *store, XentNodeId node, char const *text) {
	FluxHostMenuItemData *item = menu_item_data(store, node);
	if (item) flux_str_replace(&item->label, text);
}

void flux_host_menuitem_set_icon(FluxStore *store, XentNodeId node, char const *icon) {
	FluxHostMenuItemData *item = menu_item_data(store, node);
	if (item) flux_str_replace(&item->icon, icon);
}

void flux_host_menuitem_set_enabled(FluxStore *store, XentNodeId node, bool enabled) {
	FluxHostMenuItemData *item = menu_item_data(store, node);
	if (item) item->enabled = enabled;
}

static void menu_fly_fill(FluxHostMenuFlyData *fly) {
	if (!fly->menu) return;
	flux_menu_clear(fly->menu);
	for (XentNodeId child = xent_node_first(fly->ctx, fly->node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(fly->ctx, child))
	{
		FluxHostMenuItemData *item = menu_item_data(fly->store, child);
		if (!item) continue;
		item->parent = fly->node;
		if (!item->label) {
			flux_menu_addsep(fly->menu);
			continue;
		}
		char glyph [8] = {0};
		if (item->icon) flux_icon_to_utf8(flux_icon_lookup(item->icon), glyph, sizeof(glyph));
		flux_menu_additem(
		  fly->menu, &(FluxMenuItemDef) {
					   .type         = FLUX_MENU_ITEM_NORMAL,
					   .label        = item->label,
					   .icon_glyph   = glyph [0] ? glyph : NULL,
					   .enabled      = item->enabled,
					   .on_click     = menu_item_invoke,
					   .on_click_ctx = item,
					 }
		);
	}
}

void flux_host_menu_sync_fly(FluxStore *store, XentNodeId node) {
	FluxHostMenuFlyData *fly = menu_fly_data(store, node);
	if (!fly || !xent_node_valid(fly->ctx, node)) return;
	XentNodeId parent = flux_host_menufly_parent(store, node);
	if (parent != fly->parent) {
		menu_fly_unbind(fly, fly->parent);
		fly->parent = parent;
	}
	menu_fly_fill(fly);
	menu_fly_bind(fly, parent);
}

void flux_host_menu_sync_bar(FluxStore *store, XentNodeId node) {
	XentCtx *ctx = flux_store_ctx(store);
	if (!ctx || !xent_node_valid(ctx, node)) return;
	flux_menubar_clear_menus(store, node);
	XentNodeId content = flux_menubar_content_node(store, node);
	for (XentNodeId child = xent_node_first(ctx, content); child != XENT_NODE_INVALID;
	  child               = xent_node_next(ctx, child))
	{
		FluxHostMenuFlyData *fly = menu_fly_data(store, child);
		if (fly) flux_menubar_add_menu_ref(store, node, fly->title ? fly->title : "", fly->menu);
	}
}
