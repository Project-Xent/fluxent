#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "controls/draw/flux_control_draw.h"
#include "render/flux_fluent.h"
#include "runtime/flux_str.h"

#include "controls/factory/flux_init_nav.h"
#include "fluxent/flux_window.h"
#include "controls/data/flux_menu_bar_data.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static FluxMenuBarData *menu_bar_data(FluxStore *store, XentNodeId bar) {
	FluxNodeData *nd = flux_store_find(store, bar);
	if (!nd || nd->component_type != FLUX_CONTROL_MENU_BAR) return NULL;
	return ( FluxMenuBarData * ) nd->component_data;
}

static void menu_bar_repaint(FluxMenuBarData *d) {
	HWND h = flux_window_hwnd(d->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static void menu_bar_open(FluxMenuBarItem *it, FluxMenuInputKind input) {
	FluxMenuBarData *d = it->bar;
	if (!it->flyout) return;
	if (d->open_index == it->index) return;
	if (d->open_index >= 0) d->items [d->open_index].visual.open = false;
	if (d->open_index >= 0 && d->items [d->open_index].flyout) flux_menu_dismiss(d->items [d->open_index].flyout);
	d->open_index   = it->index;
	it->visual.open = true;
	FluxRect anchor = flux_binding_screen_anchor(d->window, d->ctx, d->store, it->node);
	flux_menu_showinput(it->flyout, anchor, FLUX_PLACEMENT_BOTTOM, input);
	menu_bar_repaint(d);
}

static void menu_bar_close(FluxMenuBarData *d) {
	if (d->open_index < 0) return;
	d->items [d->open_index].visual.open = false;
	if (d->items [d->open_index].flyout) flux_menu_dismiss(d->items [d->open_index].flyout);
	d->open_index = -1;
	menu_bar_repaint(d);
}

static void menu_bar_item_click(void *ctx) {
	FluxMenuBarItem *it = ( FluxMenuBarItem * ) ctx;
	if (it->bar->open_index == it->index) menu_bar_close(it->bar);
	else menu_bar_open(it, FLUX_MENU_INPUT_MOUSE);
}

static void menu_bar_item_hover(void *ctx, float x, float y) {
	( void ) x;
	( void ) y;
	FluxMenuBarItem *it = ( FluxMenuBarItem * ) ctx;
	if (it->bar->open_index >= 0) menu_bar_open(it, FLUX_MENU_INPUT_MOUSE);
}

static bool menu_bar_item_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxMenuBarItem *it = ( FluxMenuBarItem * ) ctx;
	switch (key) {
	case FLUX_KEY_DOWN  :
	case FLUX_KEY_ENTER :
	case FLUX_KEY_SPACE : menu_bar_open(it, FLUX_MENU_INPUT_KEYBOARD); return true;
	default             : return false;
	}
}

static void menu_bar_item_dismissed(void *ctx) {
	FluxMenuBarItem *it = ( FluxMenuBarItem * ) ctx;
	if (it->bar->open_index == it->index) {
		it->visual.open     = false;
		it->bar->open_index = -1;
		menu_bar_repaint(it->bar);
	}
}

static void menu_bar_release_item(FluxMenuBarItem *it) {
	if (it->node != XENT_NODE_INVALID && xent_node_valid(it->bar->ctx, it->node))
		flux_tree_drop(it->bar->store, it->node);
	if (it->owns_flyout && it->flyout) flux_menu_destroy(it->flyout);
	flux_str_free(it->visual.label);
	memset(it, 0, sizeof(*it));
}

void flux_menubar_clear_menus(FluxStore *store, XentNodeId bar) {
	FluxMenuBarData *d = menu_bar_data(store, bar);
	if (!d) return;
	menu_bar_close(d);
	for (int i = 0; i < d->count; i++) menu_bar_release_item(&d->items [i]);
	d->count      = 0;
	d->open_index = -1;
}

static void menu_bar_destroy(void *component_data) {
	FluxMenuBarData *d = ( FluxMenuBarData * ) component_data;
	if (!d) return;
	for (int i = 0; i < d->count; i++) {
		if (d->items [i].owns_flyout && d->items [i].flyout) flux_menu_destroy(d->items [i].flyout);
		flux_str_free(d->items [i].visual.label);
	}
	free(d);
}

XentNodeId flux_menubar_init(FluxMenuBarDesc const *info) {
	if (!info || !info->ctx || !info->store || !info->window) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_MENU_BAR);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData    *nd = flux_store_find(info->store, node);
	FluxMenuBarData *d  = nd ? ( FluxMenuBarData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}
	d->ctx        = info->ctx;
	d->store      = info->store;
	d->window     = info->window;
	d->theme      = info->theme;
	d->text       = info->text;
	d->root       = node;
	d->open_index = -1;
	d->items_host = flux_factory_create_node(info->ctx, info->store, node, FLUX_CONTROL_CONTAINER);
	d->wire_host  = flux_factory_create_node(info->ctx, info->store, node, FLUX_CONTROL_CONTAINER);
	if (d->items_host == XENT_NODE_INVALID || d->wire_host == XENT_NODE_INVALID) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	nd->component_data         = d;
	nd->destroy_component_data = menu_bar_destroy;

	xent_setproto(info->ctx, node, XENT_PROTOCOL_ABSOLUTE);
	xent_setminsize(info->ctx, node, (XentSize) {0.0f, FLUX_MENU_BAR_HEIGHT});

	xent_setproto(info->ctx, d->items_host, XENT_PROTOCOL_FLEX);
	xent_setflexdir(info->ctx, d->items_host, XENT_FLEX_ROW);
	xent_setitems(info->ctx, d->items_host, XENT_FLEX_ALIGN_CENTER);
	xent_setgap(info->ctx, d->items_host, 2.0f * FLUX_MENU_BAR_ITEM_MARGIN);
	xent_setp(
	  info->ctx, d->items_host, (XentInsets) {FLUX_MENU_BAR_ITEM_MARGIN, 0.0f, FLUX_MENU_BAR_ITEM_MARGIN, 0.0f}
	);
	xent_setwpct(info->ctx, d->items_host, 1.0f);
	xent_seth(info->ctx, d->items_host, FLUX_MENU_BAR_HEIGHT);

	xent_setproto(info->ctx, d->wire_host, XENT_PROTOCOL_ABSOLUTE);
	xent_setsize(info->ctx, d->wire_host, (XentSize) {0.0f, 0.0f});
	return node;
}

XentNodeId flux_menubar_content_node(FluxStore *store, XentNodeId bar) {
	FluxMenuBarData *d = menu_bar_data(store, bar);
	return d ? d->wire_host : XENT_NODE_INVALID;
}

static bool menu_bar_add(FluxMenuBarData *d, char const *title, FluxMenu *flyout, bool owns_flyout) {
	if (!d || d->count >= FLUX_MENU_BAR_MAX_ITEMS || !title) return false;

	XentNodeId item = flux_factory_create_node(d->ctx, d->store, d->items_host, FLUX_CONTROL_MENU_BAR_ITEM);
	if (item == XENT_NODE_INVALID) return false;

	FluxNodeData *ind = flux_store_find(d->store, item);
	if (!ind) {
		flux_tree_drop(d->store, item);
		return false;
	}

	FluxTextStyle ts = {0};
	ts.font_size     = FLUX_MENU_BAR_ITEM_FONT;
	ts.font_weight   = FLUX_FONT_REGULAR;
	float text_w     = d->text ? flux_text_measure(d->text, title, &ts, FLT_MAX).w : 0.0f;
	float item_w     = ceilf(text_w) + 2.0f * FLUX_MENU_BAR_ITEM_PAD_H;
	xent_setsize(d->ctx, item, (XentSize) {item_w, FLUX_MENU_BAR_ITEM_H});

	FluxMenuBarItem *slot             = &d->items [d->count];
	slot->bar                         = d;
	slot->node                        = item;
	slot->visual.label                = flux_str_dup(title);
	slot->flyout                      = flyout;
	slot->index                       = d->count;
	slot->owns_flyout                 = owns_flyout;

	ind->component_data               = slot;
	ind->behavior.on_click            = menu_bar_item_click;
	ind->behavior.on_click_ctx        = slot;
	ind->behavior.on_pointer_move     = menu_bar_item_hover;
	ind->behavior.on_pointer_move_ctx = slot;
	ind->behavior.on_key              = menu_bar_item_key;
	ind->behavior.on_key_ctx          = slot;

	if (flyout) flux_menu_ondismiss(flyout, menu_bar_item_dismissed, slot);

	xent_set_focusable(d->ctx, item, true);
	xent_sem_setrole(d->ctx, item, XENT_SEM_ROLE_BUTTON);
	xent_sem_setlabel(d->ctx, item, title);

	d->count++;
	return true;
}

FluxMenu *flux_menubar_add_menu(FluxStore *store, XentNodeId bar, char const *title) {
	FluxMenuBarData *d = menu_bar_data(store, bar);
	if (!d || !title) return NULL;
	FluxMenu *flyout = flux_menu_create(d->window);
	if (!flyout) return NULL;
	flux_menu_setthememanager(flyout, d->theme);
	flux_menu_settext(flyout, d->text);
	if (!menu_bar_add(d, title, flyout, true)) {
		flux_menu_destroy(flyout);
		return NULL;
	}
	return flyout;
}

bool flux_menubar_add_menu_ref(FluxStore *store, XentNodeId bar, char const *title, FluxMenu *flyout) {
	FluxMenuBarData *d = menu_bar_data(store, bar);
	return d && menu_bar_add(d, title, flyout, false);
}
