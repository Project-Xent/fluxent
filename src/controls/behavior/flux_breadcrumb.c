#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "render/flux_icon.h"
#include "runtime/flux_str.h"

#include "controls/factory/flux_init_nav.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_menu.h"
#include "fluxent/flux_window.h"
#include "controls/behavior/flux_breadcrumb_internal.h"
#include <fluxent/flux_host.h>
#include <xtk/xtk_catalog.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static FluxBreadcrumbBarData *bc_data(FluxStore *store, XentNodeId bar) {
	FluxNodeData *nd = flux_store_find(store, bar);
	if (!nd || nd->component_type != FLUX_CONTROL_BREADCRUMB_BAR) return NULL;
	return ( FluxBreadcrumbBarData * ) nd->component_data;
}

static void bc_repaint(FluxBreadcrumbBarData *d) {
	HWND h = d->window ? flux_window_hwnd(d->window) : NULL;
	if (h) InvalidateRect(h, NULL, FALSE);
}

static float bc_text_w(FluxBreadcrumbBarData *d, char const *s, float font_size) {
	if (!s || !s [0]) return 0.0f;
	if (!d->text) return 10.0f * ( float ) strlen(s);
	FluxTextStyle ts = {0};
	ts.font_size     = font_size;
	ts.font_weight   = FLUX_FONT_REGULAR;
	return flux_text_measure(d->text, s, &ts, FLT_MAX).w;
}

static float bc_glyph_w(FluxBreadcrumbBarData *d, wchar_t const *glyph, float font_size) {
	char utf8 [8];
	flux_icon_to_utf8(glyph, utf8, sizeof(utf8));
	return bc_text_w(d, utf8, font_size);
}

static void bc_measure(FluxBreadcrumbBarData *d) {
	d->chevron_cell_w = 2.0f * FLUX_BREADCRUMB_CHEVRON_PAD
	                  + ceilf(bc_glyph_w(d, FLUX_BREADCRUMB_CHEVRON_LTR, FLUX_BREADCRUMB_CHEVRON_FONT));
	d->widths [0]     = 2.0f * FLUX_BREADCRUMB_ELLIPSIS_PAD
	                  + ceilf(bc_glyph_w(d, FLUX_BREADCRUMB_ELLIPSIS, FLUX_BREADCRUMB_ELLIPSIS_FONT))
	                  + d->chevron_cell_w;

	float natural     = 0.0f;
	for (int i = 1; i <= d->count; i++) {
		float w = 2.0f * FLUX_BREADCRUMB_PAD_H + ceilf(bc_text_w(d, d->labels [i - 1], FLUX_BREADCRUMB_FONT));
		if (i < d->count) w += d->chevron_cell_w;
		d->widths [i]  = w;
		natural       += w;
	}
	d->natural_w = natural;
	xent_setsize(d->ctx, d->root, (XentSize) {natural, FLUX_BREADCRUMB_ITEM_H});
	d->widths_dirty  = false;
	d->arrange_dirty = true;
}

static void bc_place(FluxBreadcrumbBarData *d, int elem, float x, float w) {
	xent_setpos(d->ctx, d->nodes [elem], (XentPoint) {x, 0.0f});
	xent_setsize(d->ctx, d->nodes [elem], (XentSize) {w, FLUX_BREADCRUMB_ITEM_H});
	xent_set_focusable(d->ctx, d->nodes [elem], !d->disabled);
}

static void bc_hide(FluxBreadcrumbBarData *d, int elem) {
	xent_setpos(d->ctx, d->nodes [elem], (XentPoint) {0.0f, 0.0f});
	xent_setsize(d->ctx, d->nodes [elem], (XentSize) {0.0f, 0.0f});
	xent_set_focusable(d->ctx, d->nodes [elem], false);
}

static int bc_first_to_render(FluxBreadcrumbBarData const *d, float avail) {
	int   n     = d->count;
	float accum = d->widths [n] + d->widths [0];
	for (int i = n - 1; i >= 0; i--) {
		float next = accum + d->widths [i];
		if (next > avail) return i + 1;
		accum = next;
	}
	return 0;
}

static void bc_arrange(FluxBreadcrumbBarData *d, float avail) {
	int   n           = d->count;
	float accumulated = 0.0f;
	for (int i = 1; i <= n; i++) accumulated += d->widths [i];
	d->ellipsis_rendered = n > 0 && accumulated > avail;

	int first            = 1;
	if (d->ellipsis_rendered) {
		first = bc_first_to_render(d, avail);
		if (first < 1) first = 1;
	}
	d->first_rendered = first;

	float x           = 0.0f;
	if (d->ellipsis_rendered) {
		bc_place(d, 0, 0.0f, d->widths [0]);
		x = d->widths [0];
	}
	else bc_hide(d, 0);

	for (int i = 1; i <= n; i++) {
		if (i < first) {
			bc_hide(d, i);
			continue;
		}
		bc_place(d, i, x, d->widths [i]);
		x += d->widths [i];
	}
	d->arrange_dirty = false;
}

void flux_breadcrumb_sync(XentCtx *ctx, XentNodeId bar, struct FluxNodeData *nd) {
	( void ) bar;
	FluxBreadcrumbBarData *d    = ( FluxBreadcrumbBarData * ) nd->component_data;
	XentRect               rect = {0};
	if (!d || !xent_layout_rect(ctx, d->root, &rect)) return;

	if (d->widths_dirty) bc_measure(d);
	if (!d->arrange_dirty && rect.w == d->last_avail) return;
	d->last_avail = rect.w;
	bc_arrange(d, rect.w);
	bc_repaint(d);
}

static void bc_flyout_click(void *ctx) {
	FluxBreadcrumbFlyoutSlot *slot = ( FluxBreadcrumbFlyoutSlot * ) ctx;
	FluxBreadcrumbBarData    *d    = slot->bar;
	if (d->on_item_clicked) d->on_item_clicked(d->on_item_clicked_ctx, slot->index);
}

static void bc_open_flyout(FluxBreadcrumbItem *it, FluxMenuInputKind input) {
	FluxBreadcrumbBarData *d = it->bar;
	if (!d->window || !d->ellipsis_rendered) return;
	if (!d->flyout) {
		d->flyout = flux_menu_create(d->window);
		if (!d->flyout) return;
		flux_menu_setthememanager(d->flyout, d->theme);
		flux_menu_settext(d->flyout, d->text);
	}
	flux_menu_clear(d->flyout);
	int hidden = d->first_rendered - 1;
	for (int row = 0; row < hidden; row++) {
		int index            = hidden - 1 - row;
		d->fly_slots [index] = (FluxBreadcrumbFlyoutSlot) {d, index};
		flux_menu_additem(
		  d->flyout, &(FluxMenuItemDef) {
					   .type         = FLUX_MENU_ITEM_NORMAL,
					   .label        = d->labels [index],
					   .enabled      = true,
					   .on_click     = bc_flyout_click,
					   .on_click_ctx = &d->fly_slots [index],
					 }
		);
	}
	FluxRect anchor = flux_binding_screen_anchor(d->window, d->ctx, d->store, it->node);
	flux_menu_showinput(d->flyout, anchor, FLUX_PLACEMENT_BOTTOM, input);
}

static void bc_invoke(FluxBreadcrumbItem *it, FluxMenuInputKind input) {
	FluxBreadcrumbBarData *d = it->bar;
	if (d->disabled) return;
	if (it->elem == 0) {
		bc_open_flyout(it, input);
		return;
	}
	int index = it->elem - 1;
	if (d->on_item_clicked) d->on_item_clicked(d->on_item_clicked_ctx, index);
	XtkHost *host = flux_store_host(d->store);
	if (!host) return;
	XtkEvent event = {
	  .id             = XTK_EVENT_SELECT,
	  .target         = ( XtkCtrl ) d->root,
	  .source         = ( XtkCtrl ) d->root,
	  .current_target = ( XtkCtrl ) d->root,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index},
	};
	flux_host_route(host, &event);
}

static void bc_item_click(void *ctx) {
	FluxBreadcrumbItem    *it        = ( FluxBreadcrumbItem * ) ctx;
	FluxBreadcrumbBarData *d         = it->bar;
	FluxNodeData          *nd        = flux_store_find(d->store, it->node);
	float                  content_w = d->widths [it->elem] - (it->elem == d->count ? 0.0f : d->chevron_cell_w);
	if (nd && nd->hover_local_x >= 0.0f && nd->hover_local_x > content_w) return;
	bc_invoke(it, FLUX_MENU_INPUT_MOUSE);
}

static int bc_key_left_target(FluxBreadcrumbBarData const *d, int elem) {
	if (elem == 0) return -1;
	if (d->ellipsis_rendered && elem == d->first_rendered) return 0;
	return elem > 1 ? elem - 1 : -1;
}

static bool bc_item_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxBreadcrumbItem    *it = ( FluxBreadcrumbItem * ) ctx;
	FluxBreadcrumbBarData *d  = it->bar;
	switch (key) {
	case FLUX_KEY_ENTER :
	case FLUX_KEY_SPACE : bc_invoke(it, FLUX_MENU_INPUT_KEYBOARD); return true;
	case FLUX_KEY_RIGHT : {
		int next = it->elem == 0 ? d->first_rendered : it->elem + 1;
		if (next <= d->count && d->input) flux_input_setfocus(d->input, d->nodes [next], false);
		return true;
	}
	case FLUX_KEY_LEFT : {
		int prev = bc_key_left_target(d, it->elem);
		if (prev >= 0 && d->input) flux_input_setfocus(d->input, d->nodes [prev], false);
		return true;
	}
	default : return false;
	}
}

static XentNodeId bc_make_item(FluxBreadcrumbBarData *d, int elem) {
	XentNodeId node = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_BREADCRUMB_ITEM);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData       *ind = flux_store_find(d->store, node);
	FluxBreadcrumbItem *it  = ind ? ( FluxBreadcrumbItem * ) calloc(1, sizeof(*it)) : NULL;
	if (!ind || !it) {
		free(it);
		return flux_factory_fail_node(d->ctx, d->store, node);
	}
	it->bar                     = d;
	it->node                    = node;
	it->elem                    = elem;

	ind->component_data         = it;
	ind->destroy_component_data = free;
	ind->behavior.on_click      = bc_item_click;
	ind->behavior.on_click_ctx  = it;
	ind->behavior.on_key        = bc_item_key;
	ind->behavior.on_key_ctx    = it;

	xent_sem_setrole(d->ctx, node, XENT_SEM_ROLE_BUTTON);
	xent_sem_setlabel(d->ctx, node, elem > 0 ? d->labels [elem - 1] : "More");
	xent_sem_setenabled(d->ctx, node, !d->disabled);
	xent_setpos(d->ctx, node, (XentPoint) {0.0f, 0.0f});
	xent_setsize(d->ctx, node, (XentSize) {0.0f, 0.0f});
	xent_set_focusable(d->ctx, node, false);
	return node;
}

static void bc_clear_items(FluxBreadcrumbBarData *d) {
	int elems = d->nodes ? d->count + 1 : 0;
	for (int e = 0; e < elems; e++)
		if (d->nodes [e] != XENT_NODE_INVALID) flux_tree_drop(d->store, d->nodes [e]);
	int labels = d->labels ? d->count : 0;
	for (int i = 0; i < labels; i++) flux_str_free(d->labels [i]);
	free(( void * ) d->labels);
	free(d->nodes);
	free(d->widths);
	free(d->fly_slots);
	d->labels    = NULL;
	d->nodes     = NULL;
	d->widths    = NULL;
	d->fly_slots = NULL;
	d->count     = 0;
}

static bool bc_assign_items(FluxBreadcrumbBarData *d, char const *const *items, int count) {
	if (count < 0 || !items) count = 0;
	bc_clear_items(d);

	int elems    = count + 1;
	d->labels    = count ? ( char const ** ) calloc(( size_t ) count, sizeof(*d->labels)) : NULL;
	d->fly_slots = count ? ( FluxBreadcrumbFlyoutSlot * ) calloc(( size_t ) count, sizeof(*d->fly_slots)) : NULL;
	d->nodes     = ( XentNodeId * ) calloc(( size_t ) elems, sizeof(*d->nodes));
	d->widths    = ( float * ) calloc(( size_t ) elems, sizeof(*d->widths));
	if (!d->nodes || !d->widths || (count && (!d->labels || !d->fly_slots))) {
		bc_clear_items(d);
		return false;
	}
	d->count = count;
	for (int i = 0; i < count; i++) d->labels [i] = flux_str_dup(items [i]);
	for (int e = 0; e < elems; e++) d->nodes [e] = bc_make_item(d, e);

	d->ellipsis_rendered = false;
	d->first_rendered    = 1;
	d->widths_dirty      = true;
	d->arrange_dirty     = true;
	d->last_avail        = -1.0f;
	return true;
}

static void bc_destroy(void *component_data) {
	FluxBreadcrumbBarData *d = ( FluxBreadcrumbBarData * ) component_data;
	if (!d) return;
	if (d->flyout) flux_menu_destroy(d->flyout);
	if (d->labels)
		for (int i = 0; i < d->count; i++) flux_str_free(d->labels [i]);
	free(( void * ) d->labels);
	free(d->nodes);
	free(d->widths);
	free(d->fly_slots);
	free(d);
}

XentNodeId flux_breadcrumb_init(FluxBreadcrumbBarDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_BREADCRUMB_BAR);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData          *nd = flux_store_find(info->store, root);
	FluxBreadcrumbBarData *d  = nd ? ( FluxBreadcrumbBarData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}
	d->ctx                     = info->ctx;
	d->store                   = info->store;
	d->window                  = info->window;
	d->text                    = info->text;
	d->theme                   = info->theme;
	d->input                   = info->input;
	d->root                    = root;
	d->disabled                = info->disabled;
	d->first_rendered          = 1;
	d->on_item_clicked         = info->on_item_clicked;
	d->on_item_clicked_ctx     = info->userdata;

	nd->component_data         = d;
	nd->destroy_component_data = bc_destroy;

	xent_setproto(info->ctx, root, XENT_PROTOCOL_ABSOLUTE);
	xent_setsize(info->ctx, root, (XentSize) {0.0f, FLUX_BREADCRUMB_ITEM_H});
	xent_sem_setrole(info->ctx, root, XENT_SEM_ROLE_CONTAINER);

	bc_assign_items(d, info->items, info->item_count);
	return root;
}

void flux_breadcrumb_set_items(FluxStore *store, XentNodeId bar, char const *const *items, int count) {
	FluxBreadcrumbBarData *d = bc_data(store, bar);
	if (!d) return;
	if (d->flyout) flux_menu_dismiss(d->flyout);
	bc_assign_items(d, items, count);
	if (d->widths_dirty) bc_measure(d);
	bc_repaint(d);
}

void flux_breadcrumb_set_disabled(FluxStore *store, XentNodeId bar, bool disabled) {
	FluxBreadcrumbBarData *d = bc_data(store, bar);
	if (!d || d->disabled == disabled) return;
	d->disabled = disabled;
	for (int e = 0; e <= d->count; e++)
		if (d->nodes [e] != XENT_NODE_INVALID) xent_sem_setenabled(d->ctx, d->nodes [e], !disabled);
	d->arrange_dirty = true;
	bc_repaint(d);
}
