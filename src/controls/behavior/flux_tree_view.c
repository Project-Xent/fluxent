#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "render/flux_anim.h"
#include "render/flux_icon.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "controls/factory/flux_init_items.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_window.h"
#include "controls/behavior/flux_tree_internal.h"
#include <xtk/xtk_catalog.h>

#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TREE_OVERSCAN      4
#define TREE_FALLBACK_ROWS 32

static FluxTreeViewData *tree_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_TREE_VIEW || !nd->component_data) return NULL;
	return ( FluxTreeViewData * ) nd->component_data;
}

static bool  tree_valid(FluxTreeViewData const *d, int h) { return h >= 0 && h < d->node_count && d->nodes [h].in_use; }

static bool  tree_multi(FluxTreeViewData const *d) { return d->sel_mode == XTK_TREE_SELECT_MULTIPLE; }

static float tree_pitch(FluxTreeViewData const *d) { return d->row_height + 2.0f * FLUX_TREE_ROW_MARGIN_V; }

static bool  key_down(FluxKey key) { return flux_key_held(key); }

static void  tree_repaint(FluxTreeViewData *d) {
	HWND h = d->window ? flux_window_hwnd(d->window) : NULL;
	if (h) InvalidateRect(h, NULL, FALSE);
}

static bool tree_pool_reserve(FluxTreeViewData *d) {
	if (d->node_count < d->node_cap) return true;
	int           cap = d->node_cap ? d->node_cap * 2 : 32;
	FluxTreeNode *n   = ( FluxTreeNode * ) realloc(d->nodes, sizeof(*n) * ( size_t ) cap);
	if (!n) return false;
	d->nodes    = n;
	d->node_cap = cap;
	return true;
}

static int tree_node_alloc(FluxTreeViewData *d) {
	int h = d->free_head;
	if (h >= 0) d->free_head = d->nodes [h].next_sibling;
	else {
		if (!tree_pool_reserve(d)) return -1;
		h = d->node_count++;
	}
	memset(&d->nodes [h], 0, sizeof(d->nodes [h]));
	d->nodes [h].parent       = -1;
	d->nodes [h].first_child  = -1;
	d->nodes [h].next_sibling = -1;
	d->nodes [h].in_use       = true;
	return h;
}

static bool tree_flat_push(FluxTreeViewData *d, int h) {
	if (d->flat_count == d->flat_cap) {
		int  cap = d->flat_cap ? d->flat_cap * 2 : 64;
		int *f   = ( int * ) realloc(d->flat, sizeof(*f) * ( size_t ) cap);
		if (!f) return false;
		d->flat     = f;
		d->flat_cap = cap;
	}
	d->flat [d->flat_count++] = h;
	return true;
}

static void tree_flatten_chain(FluxTreeViewData *d, int first) {
	for (int c = first; c >= 0; c = d->nodes [c].next_sibling) {
		if (!tree_flat_push(d, c)) return;
		if (d->nodes [c].expanded && d->nodes [c].first_child >= 0) tree_flatten_chain(d, d->nodes [c].first_child);
	}
}

static void tree_rebuild_flat(FluxTreeViewData *d) {
	d->flat_count = 0;
	tree_flatten_chain(d, d->first_root);
	if (d->focused_flat > d->flat_count - 1) d->focused_flat = d->flat_count - 1;
}

static int tree_flat_of(FluxTreeViewData const *d, int h) {
	for (int i = 0; i < d->flat_count; i++)
		if (d->flat [i] == h) return i;
	return -1;
}

static int tree_visible_descendants(FluxTreeViewData const *d, int flat_index) {
	int16_t depth = d->nodes [d->flat [flat_index]].depth;
	int     n     = 0;
	for (int i = flat_index + 1; i < d->flat_count && d->nodes [d->flat [i]].depth > depth; i++) n++;
	return n;
}

static void tree_cascade_down(FluxTreeViewData *d, int h, uint8_t state) {
	d->nodes [h].sel_state = state;
	for (int c = d->nodes [h].first_child; c >= 0; c = d->nodes [c].next_sibling) tree_cascade_down(d, c, state);
}

static uint8_t tree_derive_state(FluxTreeViewData const *d, int h) {
	bool any_sel = false, any_unsel = false;
	for (int c = d->nodes [h].first_child; c >= 0; c = d->nodes [c].next_sibling) {
		uint8_t s = d->nodes [c].sel_state;
		if (s == FLUX_TREE_SEL_PARTIAL) return FLUX_TREE_SEL_PARTIAL;
		if (s == FLUX_TREE_SEL_SELECTED) any_sel = true;
		else any_unsel = true;
	}
	if (any_sel && any_unsel) return FLUX_TREE_SEL_PARTIAL;
	return any_sel ? FLUX_TREE_SEL_SELECTED : FLUX_TREE_SEL_NONE;
}

static void tree_update_ancestors(FluxTreeViewData *d, int h) {
	for (int p = d->nodes [h].parent; p >= 0; p = d->nodes [p].parent) {
		uint8_t s = tree_derive_state(d, p);
		if (d->nodes [p].sel_state == s) break;
		d->nodes [p].sel_state = s;
	}
}

static void tree_set_selected_multi(FluxTreeViewData *d, int h, bool selected) {
	tree_cascade_down(d, h, selected ? FLUX_TREE_SEL_SELECTED : FLUX_TREE_SEL_NONE);
	tree_update_ancestors(d, h);
}

static void       tree_item_click(void *ctx);
static void       tree_item_down(void *ctx, float x, float y, int clicks);
static bool       tree_item_key(void *ctx, FluxKey key, bool down);

static XentNodeId tree_make_row(FluxTreeViewData *d) {
	XentNodeId node = flux_factory_create_node(d->ctx, d->store, d->host, FLUX_CONTROL_TREE_ITEM);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_store_find(d->store, node);
	FluxTreeItemData *it = nd ? ( FluxTreeItemData * ) calloc(1, sizeof(FluxTreeItemData)) : NULL;
	if (!nd || !it) {
		free(it);
		return flux_factory_fail_node(d->ctx, d->store, node);
	}
	it->owner                        = d;
	it->flat_index                   = -1;

	nd->component_data               = it;
	nd->destroy_component_data       = free;
	nd->behavior.on_click            = tree_item_click;
	nd->behavior.on_click_ctx        = it;
	nd->behavior.on_pointer_down     = tree_item_down;
	nd->behavior.on_pointer_down_ctx = it;
	nd->behavior.on_key              = tree_item_key;
	nd->behavior.on_key_ctx          = it;

	xent_set_focusable(d->ctx, node, true);
	xent_sem_setrole(d->ctx, node, XENT_SEM_ROLE_BUTTON);
	xent_setwpct(d->ctx, node, 1.0f);
	xent_seth(d->ctx, node, tree_pitch(d));
	return node;
}

static void tree_window_range(FluxTreeViewData *d, int *first, int *last) {
	float offset = 0.0f, extent = 0.0f, cross = 0.0f;
	float pitch = tree_pitch(d);
	if (!flux_list_viewport(d->store, d->list, &offset, &extent, &cross) || extent <= 0.0f || pitch <= 0.0f) {
		*first = 0;
		*last  = d->flat_count < TREE_FALLBACK_ROWS ? d->flat_count - 1 : TREE_FALLBACK_ROWS - 1;
		return;
	}
	float max_offset = ( float ) d->flat_count * pitch - extent;
	if (offset > max_offset) offset = max_offset;
	if (offset < 0.0f) offset = 0.0f;

	*first = ( int ) floorf(offset / pitch) - TREE_OVERSCAN;
	*last  = ( int ) ceilf((offset + extent) / pitch) - 1 + TREE_OVERSCAN;
	if (*first < 0) *first = 0;
	if (*last > d->flat_count - 1) *last = d->flat_count - 1;
}

static bool tree_ensure_rows(FluxTreeViewData *d, int count) {
	if (count > d->row_cap) {
		int cap = d->row_cap ? d->row_cap * 2 : 16;
		if (cap < count) cap = count;
		XentNodeId *r = ( XentNodeId * ) realloc(d->rows, sizeof(*r) * ( size_t ) cap);
		if (!r) return false;
		d->rows    = r;
		d->row_cap = cap;
	}
	while (d->row_count > count) flux_tree_drop(d->store, d->rows [--d->row_count]);
	while (d->row_count < count) {
		XentNodeId row = tree_make_row(d);
		if (row == XENT_NODE_INVALID) break;
		d->rows [d->row_count++] = row;
	}
	return true;
}

static void tree_bind_row(FluxTreeViewData *d, XentNodeId row, int flat, float pitch) {
	FluxNodeData *nd = flux_store_find(d->store, row);
	if (!nd || !nd->component_data) return;
	FluxTreeItemData   *it = ( FluxTreeItemData * ) nd->component_data;
	FluxTreeNode const *n  = &d->nodes [d->flat [flat]];
	it->flat_index         = flat;
	it->press_on_chevron   = false;
	it->press_on_checkbox  = false;
	nd->render_opacity     = 1.0f;
	nd->render_translate_y = 0.0f;
	xent_setpos(d->ctx, row, (XentPoint) {0.0f, ( float ) flat * pitch});
	xent_sem_setenabled(d->ctx, row, !n->disabled);
	if (n->text) xent_sem_setlabel(d->ctx, row, n->text);
}

static void tree_realize(FluxTreeViewData *d) {
	int first = 0, last = -1;
	if (d->flat_count > 0) tree_window_range(d, &first, &last);
	int count = last - first + 1;
	if (count < 0) count = 0;
	if (!tree_ensure_rows(d, count)) return;

	float pitch = tree_pitch(d);
	for (int i = 0; i < d->row_count; i++) tree_bind_row(d, d->rows [i], first + i, pitch);
	d->win_first = first;
	d->win_count = d->row_count;
	flux_list_set_realized(d->store, d->list, first, first + d->row_count - 1, 1);
}

static XentNodeId tree_row_for_flat(FluxTreeViewData const *d, int flat) {
	if (flat < d->win_first || flat >= d->win_first + d->win_count) return XENT_NODE_INVALID;
	return d->rows [flat - d->win_first];
}

static void tree_sync_flat(FluxTreeViewData *d) {
	tree_rebuild_flat(d);
	flux_list_set_extent(d->store, d->list, d->flat_count, tree_pitch(d), 0.0f, 1);
	tree_realize(d);
}

static void tree_apply_entrance_row(FluxTreeViewData *d, int i, float p) {
	FluxNodeData *nd = flux_store_find(d->store, d->rows [i]);
	if (!nd) return;
	int  flat              = d->win_first + i;
	bool entering          = flat >= d->anim_first && flat < d->anim_first + d->anim_count;
	nd->render_opacity     = entering ? p : 1.0f;
	nd->render_translate_y = entering ? (1.0f - p) * FLUX_TREE_ENTRANCE_DY : 0.0f;
}

static bool tree_tick_entrance(FluxTreeViewData *d, DWORD now) {
	if (d->anim_count <= 0) return false;
	float t       = ( float ) (now - d->anim_start) / FLUX_TREE_ENTRANCE_MS;
	bool  running = t < 1.0f;
	if (t < 0.0f) t = 0.0f;
	if (!running) t = 1.0f;
	float p = flux_cubic_bezier(t, 0.0f, 0.0f, 0.0f, 1.0f);
	for (int i = 0; i < d->row_count; i++) tree_apply_entrance_row(d, i, p);
	if (!running) d->anim_count = 0;
	return running;
}

static bool tree_step(void *ctx, unsigned long now) {
	FluxTreeViewData *d = ( FluxTreeViewData * ) ctx;
	if (d->realize_pending) {
		d->realize_pending = false;
		tree_realize(d);
	}
	bool active = tree_tick_entrance(d, ( DWORD ) now);
	tree_repaint(d);
	return active || d->realize_pending;
}

static void tree_on_stale(void *ctx) {
	FluxTreeViewData *d = ( FluxTreeViewData * ) ctx;
	if (!d->window) {
		tree_realize(d);
		return;
	}
	d->realize_pending = true;
	flux_anim_register(d, tree_step);
}

static void tree_route(FluxTreeViewData *d, XtkEventId id, XtkEventPayload payload) {
	XtkHost *host = flux_store_host(d->store);
	if (!host) return;
	XtkEvent event = {
	  .id             = id,
	  .target         = ( XtkCtrl ) d->root,
	  .source         = ( XtkCtrl ) d->root,
	  .current_target = ( XtkCtrl ) d->root,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	  .payload        = payload,
	};
	flux_host_route(host, &event);
}

static XtkEventPayload tree_state_payload(int flat, bool value) {
	return (XtkEventPayload) {
	  .kind         = XTK_EVENT_PAYLOAD_INDEX_BOOL,
	  .u.index_bool = {.index = flat, .value = value},
	};
}

static void tree_toggle_expand(FluxTreeViewData *d, int flat) {
	int           h = d->flat [flat];
	FluxTreeNode *n = &d->nodes [h];
	if (n->first_child < 0) return;
	n->expanded = !n->expanded;
	tree_sync_flat(d);

	d->anim_count = 0;
	if (n->expanded && d->window) {
		d->anim_first = flat + 1;
		d->anim_count = tree_visible_descendants(d, flat);
		d->anim_start = flux_anim_now();
		if (d->anim_count > 0) {
			flux_anim_register(d, tree_step);
			tree_tick_entrance(d, d->anim_start);
		}
	}
	tree_repaint(d);
	tree_route(d, XTK_EVENT_TREE_EXPAND, tree_state_payload(flat, n->expanded));
	if (d->on_expand) d->on_expand(d->expand_ctx, flat, n->expanded);
}

static void tree_select_single(FluxTreeViewData *d, int flat) {
	if (d->sel_mode != XTK_TREE_SELECT_SINGLE) return;
	int h = d->flat [flat];
	if (d->selected == h || d->nodes [h].disabled) return;
	d->selected = h;
	tree_repaint(d);
	tree_route(d, XTK_EVENT_TREE_SELECT, tree_state_payload(flat, true));
	if (d->on_select) d->on_select(d->select_ctx, flat, true);
}

static void tree_toggle_check(FluxTreeViewData *d, int flat) {
	int  h   = d->flat [flat];
	bool now = d->nodes [h].sel_state != FLUX_TREE_SEL_SELECTED;
	tree_set_selected_multi(d, h, now);
	tree_repaint(d);
	tree_route(d, XTK_EVENT_TREE_SELECT, tree_state_payload(flat, now));
	if (d->on_select) d->on_select(d->select_ctx, flat, now);
}

static void tree_focus_flat(FluxTreeViewData *d, int flat) {
	if (flat < 0) flat = 0;
	if (flat > d->flat_count - 1) flat = d->flat_count - 1;
	if (flat < 0) return;

	d->focused_flat = flat;
	flux_list_scrollto(d->store, d->list, flat);
	tree_realize(d);

	XentNodeId row = tree_row_for_flat(d, flat);
	tree_repaint(d);
	if (row != XENT_NODE_INVALID && d->input) flux_input_setfocus(d->input, row, false);
}

static int tree_page_rows(FluxTreeViewData *d) {
	float offset = 0.0f, extent = 0.0f, cross = 0.0f;
	if (flux_list_viewport(d->store, d->list, &offset, &extent, &cross) && tree_pitch(d) > 0.0f) {
		int rows = ( int ) (extent / tree_pitch(d));
		if (rows > 0) return rows;
	}
	return 8;
}

static int tree_nav_delta(FluxTreeViewData *d, int flat, FluxKey key) {
	switch (key) {
	case FLUX_KEY_UP        : return flat - 1;
	case FLUX_KEY_DOWN      : return flat + 1;
	case FLUX_KEY_HOME      : return 0;
	case FLUX_KEY_END       : return d->flat_count - 1;
	case FLUX_KEY_PAGE_UP   : return flat - tree_page_rows(d);
	case FLUX_KEY_PAGE_DOWN : return flat + tree_page_rows(d);
	default                 : return flat;
	}
}

static bool tree_key_nav(FluxTreeViewData *d, int flat, FluxKey key) {
	int target = tree_nav_delta(d, flat, key);
	if (target < 0) target = 0;
	if (target > d->flat_count - 1) target = d->flat_count - 1;
	if (target == flat && key != FLUX_KEY_HOME && key != FLUX_KEY_END) return true;
	FluxStore *store = d->store;
	XentNodeId root  = d->root;
	if (!key_down(FLUX_KEY_CONTROL)) tree_select_single(d, target);
	d = tree_data(store, root);
	if (d) tree_focus_flat(d, target);
	return true;
}

static bool tree_key_right(FluxTreeViewData *d, FluxTreeNode const *n, int flat) {
	if (n->first_child < 0) return false;
	if (!n->expanded) {
		tree_toggle_expand(d, flat);
		return true;
	}
	FluxStore *store = d->store;
	XentNodeId root  = d->root;
	tree_select_single(d, flat + 1);
	d = tree_data(store, root);
	if (d) tree_focus_flat(d, flat + 1);
	return true;
}

static bool tree_key_left(FluxTreeViewData *d, FluxTreeNode const *n, int flat) {
	if (n->expanded && n->first_child >= 0) {
		tree_toggle_expand(d, flat);
		return true;
	}
	if (n->parent < 0) return false;
	int pf = tree_flat_of(d, n->parent);
	if (pf < 0) return true;
	FluxStore *store = d->store;
	XentNodeId root  = d->root;
	tree_select_single(d, pf);
	d = tree_data(store, root);
	if (d) tree_focus_flat(d, pf);
	return true;
}

static bool tree_key_check(FluxTreeViewData *d, FluxTreeNode const *n, int flat) {
	if (!n->disabled) tree_toggle_check(d, flat);
	return true;
}

static bool tree_key_invoke(FluxTreeViewData *d, FluxTreeNode const *n, int flat) {
	if (!n->disabled) {
		FluxStore *store = d->store;
		XentNodeId root  = d->root;
		tree_select_single(d, flat);
		d = tree_data(store, root);
		if (!d) return true;
		tree_route(d, XTK_EVENT_INVOKE, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = flat});
		if (d->on_invoke) d->on_invoke(d->invoke_ctx, flat);
	}
	return true;
}

static bool tree_key_space(FluxTreeViewData *d, FluxTreeNode const *n, int flat) {
	if (tree_multi(d)) return tree_key_check(d, n, flat);
	return tree_key_invoke(d, n, flat);
}

static bool tree_key_is_nav(FluxKey key) {
	return key == FLUX_KEY_UP
	    || key == FLUX_KEY_DOWN
	    || key == FLUX_KEY_HOME
	    || key == FLUX_KEY_END
	    || key == FLUX_KEY_PAGE_UP
	    || key == FLUX_KEY_PAGE_DOWN;
}

static bool tree_item_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxTreeItemData *it   = ( FluxTreeItemData * ) ctx;
	FluxTreeViewData *d    = it->owner;
	int               flat = it->flat_index;
	if (!d || flat < 0 || flat >= d->flat_count) return false;
	FluxTreeNode const *n = &d->nodes [d->flat [flat]];

	if (tree_key_is_nav(key)) return tree_key_nav(d, flat, key);
	if (key == FLUX_KEY_SPACE) return tree_key_space(d, n, flat);
	if (key == FLUX_KEY_ENTER) return tree_key_invoke(d, n, flat);
	bool plain = !key_down(FLUX_KEY_CONTROL) && !key_down(FLUX_KEY_SHIFT) && !key_down(FLUX_KEY_ALT);
	if (key == FLUX_KEY_RIGHT) return plain && tree_key_right(d, n, flat);
	if (key == FLUX_KEY_LEFT) return plain && tree_key_left(d, n, flat);
	return false;
}

static void tree_item_down(void *ctx, float x, float y, int clicks) {
	( void ) y;
	( void ) clicks;
	FluxTreeItemData *it   = ( FluxTreeItemData * ) ctx;
	FluxTreeViewData *d    = it->owner;
	int               flat = it->flat_index;
	it->press_on_chevron   = false;
	it->press_on_checkbox  = false;
	if (!d || flat < 0 || flat >= d->flat_count) return;
	FluxTreeNode const *n = &d->nodes [d->flat [flat]];
	if (n->disabled) return;

	float ix = FLUX_TREE_ROW_MARGIN_H + FLUX_TREE_INDENT * ( float ) n->depth;
	bool  chevron;
	if (tree_multi(d)) {
		if (x >= ix + FLUX_TREE_CHECK_MARGIN_L && x < ix + FLUX_TREE_MULTI_CHEVRON_X) {
			it->press_on_checkbox = true;
			return;
		}
		chevron
		  = x >= ix + FLUX_TREE_MULTI_CHEVRON_X && x < ix + FLUX_TREE_MULTI_CHEVRON_X + FLUX_TREE_CHEVRON_ZONE_MULTI;
	}
	else chevron = x >= ix && x < ix + FLUX_TREE_CHEVRON_ZONE;

	if (chevron && n->first_child >= 0) {
		it->press_on_chevron = true;
		tree_toggle_expand(d, flat);
	}
}

static void tree_item_click(void *ctx) {
	FluxTreeItemData *it       = ( FluxTreeItemData * ) ctx;
	FluxTreeViewData *d        = it->owner;
	int               flat     = it->flat_index;
	bool              chevron  = it->press_on_chevron;
	bool              checkbox = it->press_on_checkbox;
	it->press_on_chevron       = false;
	it->press_on_checkbox      = false;
	if (!d || flat < 0 || flat >= d->flat_count) return;
	FluxTreeNode const *n = &d->nodes [d->flat [flat]];
	if (n->disabled || chevron) return;

	d->focused_flat = flat;
	if (checkbox && tree_multi(d)) {
		tree_toggle_check(d, flat);
		return;
	}
	FluxStore *store = d->store;
	XentNodeId root  = d->root;
	tree_select_single(d, flat);
	d = tree_data(store, root);
	if (!d) return;
	tree_route(d, XTK_EVENT_INVOKE, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = flat});
	if (d->on_invoke) d->on_invoke(d->invoke_ctx, flat);
}

static void tree_destroy(void *component_data) {
	FluxTreeViewData *d = ( FluxTreeViewData * ) component_data;
	if (!d) return;
	flux_anim_unregister(d);
	for (int i = 0; i < d->node_count; i++) {
		if (!d->nodes [i].in_use) continue;
		flux_str_free(d->nodes [i].text);
		flux_str_free(d->nodes [i].icon_name);
	}
	free(d->nodes);
	free(d->flat);
	free(d->rows);
	free(d);
}

XentNodeId flux_tree_init(FluxTreeViewDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_TREE_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_store_find(info->store, root);
	FluxTreeViewData *d  = nd ? ( FluxTreeViewData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	xent_setproto(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_setflexdir(info->ctx, root, XENT_FLEX_COLUMN);
	xent_setitems(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);

	XentNodeId list = flux_list_init(&(FluxListViewDesc) {
	  .ctx = info->ctx, .store = info->store, .parent = root, .kind = XTK_LIST_KIND_LIST});
	if (list == XENT_NODE_INVALID) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}
	xent_setgrow(info->ctx, list, 1.0f);
	xent_setbasis(info->ctx, list, 0.0f);

	d->ctx                     = info->ctx;
	d->store                   = info->store;
	d->window                  = info->window;
	d->input                   = info->input;
	d->root                    = root;
	d->list                    = list;
	d->host                    = flux_list_content_node(info->store, list);
	d->free_head               = -1;
	d->first_root              = -1;
	d->sel_mode                = info->selection_mode;
	d->selected                = -1;
	d->row_height              = info->row_height > 0.0f ? info->row_height : FLUX_TREE_ROW_H;
	d->focused_flat            = -1;
	d->on_invoke               = info->on_invoke;
	d->invoke_ctx              = info->invoke_ctx;
	d->on_expand               = info->on_expand;
	d->expand_ctx              = info->expand_ctx;
	d->on_select               = info->on_select;
	d->select_ctx              = info->select_ctx;

	nd->component_data         = d;
	nd->destroy_component_data = tree_destroy;

	flux_list_onstale(info->store, list, tree_on_stale, d);
	xent_sem_setrole(info->ctx, root, XENT_SEM_ROLE_CONTAINER);
	return root;
}

static int tree_add_node(FluxTreeViewData *d, int parent, char const *text, char const *icon) {
	if (parent >= 0 && !tree_valid(d, parent)) return -1;
	int h = tree_node_alloc(d);
	if (h < 0) return -1;

	FluxTreeNode *n  = &d->nodes [h];
	n->parent        = parent;
	n->text          = flux_str_dup(text);
	n->icon_name     = flux_str_dup(icon);
	wchar_t const *g = flux_icon_lookup(icon);
	n->glyph         = g && g [0] ? ( uint32_t ) g [0] : 0;
	n->depth         = parent >= 0 ? ( int16_t ) (d->nodes [parent].depth + 1) : 0;
	if (tree_multi(d) && parent >= 0 && d->nodes [parent].sel_state == FLUX_TREE_SEL_SELECTED)
		n->sel_state = FLUX_TREE_SEL_SELECTED;

	int *link = parent >= 0 ? &d->nodes [parent].first_child : &d->first_root;
	while (*link >= 0) link = &d->nodes [*link].next_sibling;
	*link = h;
	return h;
}

int flux_tree_add_node(FluxStore *store, XentNodeId tree, int parent, char const *text, char const *icon) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d) return -1;
	int h = tree_add_node(d, parent, text, icon);
	if (h < 0) return -1;
	tree_sync_flat(d);
	tree_repaint(d);
	return h;
}

static void tree_free_subtree(FluxTreeViewData *d, int h) {
	for (int c = d->nodes [h].first_child; c >= 0;) {
		int next = d->nodes [c].next_sibling;
		tree_free_subtree(d, c);
		c = next;
	}
	if (d->selected == h) d->selected = -1;
	flux_str_free(d->nodes [h].text);
	flux_str_free(d->nodes [h].icon_name);
	d->nodes [h].in_use       = false;
	d->nodes [h].next_sibling = d->free_head;
	d->free_head              = h;
}

static void tree_model_dispose(FluxTreeViewData *d) {
	while (d->first_root >= 0) {
		int root      = d->first_root;
		d->first_root = d->nodes [root].next_sibling;
		tree_free_subtree(d, root);
	}
	free(d->nodes);
	d->nodes = NULL;
}

static bool tree_model_build(FluxTreeViewData *next, XtkTreeModelView const *model, int *handles) {
	for (uint32_t i = 0; i < model->count; i++) {
		XtkTreeModelItemView item;
		if (!xtk_treemodel_item(model, i, &item)) return false;
		int parent = item.parent < 0 ? -1 : handles [item.parent];
		int handle = tree_add_node(next, parent, item.text, item.icon);
		if (handle < 0) return false;
		next->nodes [handle].expanded = (item.flags & XTK_TREE_MODEL_EXPANDED) != 0u;
		next->nodes [handle].disabled = (item.flags & XTK_TREE_MODEL_DISABLED) != 0u;
		next->nodes [handle].sel_state
		  = (item.flags & XTK_TREE_MODEL_CHECKED) ? FLUX_TREE_SEL_SELECTED : FLUX_TREE_SEL_NONE;
		handles [i] = handle;
	}
	for (uint32_t i = model->count; i > 0u; i--) {
		int h = handles [i - 1u];
		if (next->nodes [h].first_child >= 0) next->nodes [h].sel_state = tree_derive_state(next, h);
	}
	return true;
}

static void tree_model_adopt(FluxTreeViewData *d, FluxTreeViewData *next) {
	tree_model_dispose(d);
	d->nodes      = next->nodes;
	d->node_count = next->node_count;
	d->node_cap   = next->node_cap;
	d->free_head  = next->free_head;
	d->first_root = next->first_root;
	d->selected   = -1;
	next->nodes   = NULL;
}

bool flux_tree_set_model(FluxStore *store, XentNodeId tree, XtkTreeModelView const *model) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || !model || model->count > INT_MAX) return false;
	int *handles = model->count ? ( int * ) malloc(sizeof(*handles) * model->count) : NULL;
	if (model->count && !handles) return false;
	FluxTreeViewData next = {.free_head = -1, .first_root = -1, .selected = -1, .sel_mode = d->sel_mode};
	bool             ok   = tree_model_build(&next, model, handles);
	free(handles);
	if (!ok) {
		tree_model_dispose(&next);
		return false;
	}
	tree_model_adopt(d, &next);
	tree_sync_flat(d);
	tree_repaint(d);
	return true;
}

void flux_tree_set_expanded(FluxStore *store, XentNodeId tree, int node, bool expanded) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || !tree_valid(d, node) || d->nodes [node].expanded == expanded) return;
	d->nodes [node].expanded = expanded;
	tree_sync_flat(d);
	tree_repaint(d);
}

void flux_tree_setmode(FluxStore *store, XentNodeId tree, XtkTreeSelMode mode) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || d->sel_mode == mode) return;
	d->sel_mode = mode;
	d->selected = -1;
	for (int i = 0; i < d->node_count; i++)
		if (d->nodes [i].in_use) d->nodes [i].sel_state = FLUX_TREE_SEL_NONE;
	tree_repaint(d);
}

void flux_tree_set_selected(FluxStore *store, XentNodeId tree, int node, bool selected) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d) return;
	if (d->sel_mode == XTK_TREE_SELECT_SINGLE && node < 0) {
		d->selected = -1;
		tree_repaint(d);
		return;
	}
	if (!tree_valid(d, node)) return;
	if (d->sel_mode == XTK_TREE_SELECT_SINGLE) d->selected = selected ? node : (d->selected == node ? -1 : d->selected);
	else if (tree_multi(d)) tree_set_selected_multi(d, node, selected);
	tree_repaint(d);
}

int flux_tree_flat_node(FluxStore *store, XentNodeId tree, int flat_index) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || flat_index < 0 || flat_index >= d->flat_count) return -1;
	return d->flat [flat_index];
}
