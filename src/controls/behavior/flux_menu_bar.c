#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"
#include "render/flux_fluent.h"
#include "runtime/flux_str.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"
#include "fluxent/controls/flux_menu_bar_data.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <windows.h>

/* WinUI MenuBar (MenuBar.cpp / MenuBarItem.cpp). A horizontal stack of items,
 * each owning a MenuFlyout. At most one flyout is open; while one is open,
 * moving onto another item switches to it (OnMenuBarItemPointerEntered), and a
 * fresh press opens the pressed item (OnMenuBarItemPointerPressed). Down/Enter/
 * Space open the focused item; Left/Right fall through to the app's built-in
 * sibling focus navigation, matching the bar's XYFocus arrangement. */

static FluxMenuBarData *menu_bar_data(FluxNodeStore *store, XentNodeId bar) {
	FluxNodeData *nd = flux_node_store_get(store, bar);
	if (!nd || nd->component_type != FLUX_CONTROL_MENU_BAR) return NULL;
	return ( FluxMenuBarData * ) nd->component_data;
}

static void menu_bar_repaint(FluxMenuBarData *d) {
	HWND h = flux_window_hwnd(d->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static void menu_bar_open(FluxMenuBarItem *it, FluxMenuInputKind input) {
	FluxMenuBarData *d = it->bar;
	if (d->open_index == it->index) return;
	if (d->open_index >= 0) flux_menu_flyout_dismiss(d->items [d->open_index].flyout);
	d->open_index   = it->index;
	FluxRect anchor = flux_binding_screen_anchor(d->window, d->ctx, d->store, it->node);
	flux_menu_flyout_show_for_input(it->flyout, anchor, FLUX_PLACEMENT_BOTTOM, input);
	menu_bar_repaint(d);
}

static void menu_bar_close(FluxMenuBarData *d) {
	if (d->open_index < 0) return;
	flux_menu_flyout_dismiss(d->items [d->open_index].flyout);
	d->open_index = -1;
	menu_bar_repaint(d);
}

static void menu_bar_item_click(void *ctx) {
	FluxMenuBarItem *it = ( FluxMenuBarItem * ) ctx;
	if (it->bar->open_index == it->index) menu_bar_close(it->bar);
	else menu_bar_open(it, FLUX_MENU_INPUT_MOUSE);
}

/* Hovering an item while another item's flyout is open switches to it. */
static void menu_bar_item_hover(void *ctx, float x, float y) {
	( void ) x;
	( void ) y;
	FluxMenuBarItem *it = ( FluxMenuBarItem * ) ctx;
	if (it->bar->open_index >= 0) menu_bar_open(it, FLUX_MENU_INPUT_MOUSE);
}

static bool menu_bar_item_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxMenuBarItem *it = ( FluxMenuBarItem * ) ctx;
	switch (vk) {
	case VK_DOWN :
	case VK_RETURN :
	case VK_SPACE  : menu_bar_open(it, FLUX_MENU_INPUT_KEYBOARD); return true;
	default        : return false; /* Left/Right -> built-in sibling focus nav. */
	}
}

/* Light-dismiss or item activation closed the flyout from the menu side. */
static void menu_bar_item_dismissed(void *ctx) {
	FluxMenuBarItem *it = ( FluxMenuBarItem * ) ctx;
	if (it->bar->open_index == it->index) {
		it->bar->open_index = -1;
		menu_bar_repaint(it->bar);
	}
}

static void menu_bar_destroy(void *component_data) {
	FluxMenuBarData *d = ( FluxMenuBarData * ) component_data;
	if (!d) return;
	for (int i = 0; i < d->count; i++) {
		if (d->items [i].flyout) flux_menu_flyout_destroy(d->items [i].flyout);
		flux_str_free(d->items [i].label);
	}
	free(d);
}

XentNodeId flux_create_menu_bar(FluxMenuBarCreateInfo const *info) {
	if (!info || !info->ctx || !info->store || !info->window) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_MENU_BAR);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData    *nd = flux_node_store_get(info->store, node);
	FluxMenuBarData *d  = nd ? ( FluxMenuBarData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}
	d->ctx                     = info->ctx;
	d->store                   = info->store;
	d->window                  = info->window;
	d->theme                   = info->theme;
	d->text                    = info->text;
	d->root                    = node;
	d->open_index              = -1;

	nd->component_data         = d;
	nd->destroy_component_data = menu_bar_destroy;

	xent_set_protocol(info->ctx, node, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, node, XENT_FLEX_ROW);
	xent_set_flex_align_items(info->ctx, node, XENT_FLEX_ALIGN_CENTER);
	xent_set_gap(info->ctx, node, 2.0f * FLUX_MENU_BAR_ITEM_MARGIN);
	xent_set_padding(info->ctx, node, (XentInsets) {FLUX_MENU_BAR_ITEM_MARGIN, 0.0f, FLUX_MENU_BAR_ITEM_MARGIN, 0.0f});
	xent_set_min_size(info->ctx, node, (XentSize) {0.0f, FLUX_MENU_BAR_HEIGHT});
	return node;
}

FluxMenuFlyout *flux_menu_bar_add_menu(FluxNodeStore *store, XentNodeId bar, char const *title) {
	FluxMenuBarData *d = menu_bar_data(store, bar);
	if (!d || d->count >= FLUX_MENU_BAR_MAX_ITEMS || !title) return NULL;

	XentNodeId item = flux_factory_create_node(d->ctx, d->store, bar, FLUX_CONTROL_MENU_BAR_ITEM);
	if (item == XENT_NODE_INVALID) return NULL;

	FluxMenuFlyout *flyout = flux_menu_flyout_create(d->window);
	FluxNodeData   *ind    = flux_node_store_get(d->store, item);
	if (!flyout || !ind) {
		if (flyout) flux_menu_flyout_destroy(flyout);
		return NULL;
	}
	flux_menu_flyout_set_theme_manager(flyout, d->theme);
	flux_menu_flyout_set_text_renderer(flyout, d->text);

	FluxTextStyle ts = {0};
	ts.font_size     = FLUX_MENU_BAR_ITEM_FONT;
	ts.font_weight   = FLUX_FONT_REGULAR;
	float text_w     = d->text ? flux_text_measure(d->text, title, &ts, FLT_MAX).w : 0.0f;
	float item_w     = ceilf(text_w) + 2.0f * FLUX_MENU_BAR_ITEM_PAD_H;
	xent_set_size(d->ctx, item, (XentSize) {item_w, FLUX_MENU_BAR_ITEM_H});

	FluxMenuBarItem *slot             = &d->items [d->count];
	slot->bar                         = d;
	slot->node                        = item;
	slot->label                       = flux_str_dup(title);
	slot->flyout                      = flyout;
	slot->index                       = d->count;

	ind->component_data               = slot;
	ind->behavior.on_click            = menu_bar_item_click;
	ind->behavior.on_click_ctx        = slot;
	ind->behavior.on_pointer_move     = menu_bar_item_hover;
	ind->behavior.on_pointer_move_ctx = slot;
	ind->behavior.on_key              = menu_bar_item_key;
	ind->behavior.on_key_ctx          = slot;

	flux_menu_flyout_set_dismiss_callback(flyout, menu_bar_item_dismissed, slot);

	xent_set_focusable(d->ctx, item, true);
	xent_set_semantic_role(d->ctx, item, XENT_SEMANTIC_BUTTON);
	xent_set_semantic_label(d->ctx, item, title);

	d->count++;
	return flyout;
}
