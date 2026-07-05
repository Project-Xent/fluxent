/**
 * @file flux_tree_view.c
 * @brief WinUI 3 TreeView: retained tree model, the flattened visible-node
 * list (ViewModel), tri-state cascade selection, keyboard state machine, and
 * control-owned row virtualization on top of the ListView spine.
 *
 * Architecture (see flux_tree_view_data.h): the root owns the node pool and
 * the flat array; an inner FLUX_CONTROL_LIST spine supplies the scroll
 * viewport, the host extent, and the per-frame staleness watch. Rows are
 * recycled FLUX_CONTROL_TREE_ITEM leaves positioned absolutely at
 * flat_index × pitch — the row window shifts by rebinding flat indices, not
 * by node churn.
 *
 * Interaction semantics follow TreeViewItem.cpp: the 40px-wide padded
 * chevron zone toggles is_expanded on pointer *down* (handled — it never
 * selects/invokes); row content fires ItemInvoked on release and selects in
 * Single mode; the Multiple-mode checkbox toggles the tri-state cascade on
 * release / Space only (row clicks never toggle it). Left/Right collapse or
 * expand-or-hop per OnKeyDown; plain arrows only (Ctrl moves focus without
 * selecting; Shift+Alt reorder is not implemented).
 */
#include "controls/factory/flux_factory.h"
#include "render/flux_anim.h"
#include "render/flux_icon.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_window.h"
#include "fluxent/controls/flux_tree_view_data.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TREE_OVERSCAN      4  /* extra rows realized beyond the viewport (xtk list default) */
#define TREE_FALLBACK_ROWS 32 /* first window before layout has produced a viewport */

static FluxTreeViewData *tree_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_TREE_VIEW || !nd->component_data) return NULL;
	return ( FluxTreeViewData * ) nd->component_data;
}

static bool tree_valid(FluxTreeViewData const *d, int h) { return h >= 0 && h < d->node_count && d->nodes [h].in_use; }

static bool tree_multi(FluxTreeViewData const *d) { return d->sel_mode == XTK_TREE_SELECT_MULTIPLE; }

/* Row pitch: 28-tall pill + 2+2 vertical margins = 32 (TreeViewItemPresenterMargin). */
static float tree_pitch(FluxTreeViewData const *d) { return d->row_height + 2.0f * FLUX_TREE_ROW_MARGIN_V; }

static bool key_down(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }

static void tree_repaint(FluxTreeViewData *d) {
	HWND h = d->window ? flux_window_hwnd(d->window) : NULL;
	if (h) InvalidateRect(h, NULL, FALSE);
}

/* -------------------------------------------------------------------------
 * Node pool (handles are pool indices; free slots chain through next_sibling)
 * ---------------------------------------------------------------------- */

/* Grow the pool when full; false = allocation failure. */
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

/* -------------------------------------------------------------------------
 * Flatten (ViewModel.cpp): a node is visible iff every ancestor is expanded;
 * the flat list is the pre-order DFS of the expanded tree.
 * ---------------------------------------------------------------------- */

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

/* Rows currently visible under flat_index (depth > node.depth run). */
static int tree_visible_descendants(FluxTreeViewData const *d, int flat_index) {
	int16_t depth = d->nodes [d->flat [flat_index]].depth;
	int     n     = 0;
	for (int i = flat_index + 1; i < d->flat_count && d->nodes [d->flat [i]].depth > depth; i++) n++;
	return n;
}

/* -------------------------------------------------------------------------
 * Tri-state cascade selection (ViewModel.cpp UpdateSelection)
 * ---------------------------------------------------------------------- */

static void tree_cascade_down(FluxTreeViewData *d, int h, uint8_t state) {
	d->nodes [h].sel_state = state;
	for (int c = d->nodes [h].first_child; c >= 0; c = d->nodes [c].next_sibling) tree_cascade_down(d, c, state);
}

/* Any Partial child, or a mix of Selected + UnSelected → Partial; else
 * Selected when at least one child is selected; else UnSelected. */
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

/* Re-derive ancestors bottom-up, stopping at the first unchanged state. */
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

/* -------------------------------------------------------------------------
 * Row realization (control-owned recycling over the LIST spine)
 * ---------------------------------------------------------------------- */

static void tree_item_click(void *ctx);
static void tree_item_down(void *ctx, float x, float y, int clicks);
static bool tree_item_key(void *ctx, unsigned int vk, bool down);

static XentNodeId tree_make_row(FluxTreeViewData *d) {
	XentNodeId node = flux_factory_create_node(d->ctx, d->store, d->host, FLUX_CONTROL_TREE_ITEM);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_node_store_get(d->store, node);
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
	xent_set_semantic_role(d->ctx, node, XENT_SEMANTIC_BUTTON);
	xent_set_width_percent(d->ctx, node, 1.0f);
	xent_set_height(d->ctx, node, tree_pitch(d));
	return node;
}

/* Realized flat window: viewport rows + overscan (a plausible first window
 * before layout has produced a viewport; the stale watch corrects it). */
static void tree_window_range(FluxTreeViewData *d, int *first, int *last) {
	float offset = 0.0f, extent = 0.0f, cross = 0.0f;
	float pitch = tree_pitch(d);
	if (!flux_list_view_viewport(d->store, d->list, &offset, &extent, &cross) || extent <= 0.0f || pitch <= 0.0f) {
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

/* (Re)bind the recycled rows to the current window: position each at
 * flat × pitch and refresh its semantics; set_realized re-arms the stale
 * watch. Entrance transforms are reset here and re-applied by the tick. */
static void tree_realize(FluxTreeViewData *d) {
	int first = 0, last = -1;
	if (d->flat_count > 0) tree_window_range(d, &first, &last);
	int count = last - first + 1;
	if (count < 0) count = 0;

	if (count > d->row_cap) {
		int         cap = d->row_cap ? d->row_cap * 2 : 16;
		if (cap < count) cap = count;
		XentNodeId *r   = ( XentNodeId * ) realloc(d->rows, sizeof(*r) * ( size_t ) cap);
		if (!r) return;
		d->rows    = r;
		d->row_cap = cap;
	}
	while (d->row_count > count) flux_subtree_destroy(d->store, d->rows [--d->row_count]);
	while (d->row_count < count) {
		XentNodeId row = tree_make_row(d);
		if (row == XENT_NODE_INVALID) break;
		d->rows [d->row_count++] = row;
	}

	float pitch = tree_pitch(d);
	for (int i = 0; i < d->row_count; i++) {
		int           flat = first + i;
		FluxNodeData *nd   = flux_node_store_get(d->store, d->rows [i]);
		if (!nd || !nd->component_data) continue;
		FluxTreeItemData *it   = ( FluxTreeItemData * ) nd->component_data;
		FluxTreeNode const *n  = &d->nodes [d->flat [flat]];
		it->flat_index         = flat;
		it->press_on_chevron   = false;
		it->press_on_checkbox  = false;
		nd->render_opacity     = 1.0f;
		nd->render_translate_y = 0.0f;
		xent_set_absolute_position(d->ctx, d->rows [i], (XentPoint) {0.0f, ( float ) flat * pitch});
		xent_set_semantic_enabled(d->ctx, d->rows [i], !n->disabled);
		if (n->text) xent_set_semantic_label(d->ctx, d->rows [i], n->text);
	}
	d->win_first = first;
	d->win_count = d->row_count;
	flux_list_view_set_realized(d->store, d->list, first, first + d->row_count - 1, 1);
}

static XentNodeId tree_row_for_flat(FluxTreeViewData const *d, int flat) {
	if (flat < d->win_first || flat >= d->win_first + d->win_count) return XENT_NODE_INVALID;
	return d->rows [flat - d->win_first];
}

/* Model changed: rebuild the flat list, resize the host extent, re-realize. */
static void tree_sync_flat(FluxTreeViewData *d) {
	tree_rebuild_flat(d);
	flux_list_view_set_extent(d->store, d->list, d->flat_count, tree_pitch(d), 0.0f, 1);
	tree_realize(d);
}

/* -------------------------------------------------------------------------
 * Expand entrance animation + deferred re-realize (shared anim step)
 * ---------------------------------------------------------------------- */

/* New rows fade in and slide up (EntranceThemeTransition, staggering off);
 * everything already on screen snaps — collapse is instant removal. */
static bool tree_tick_entrance(FluxTreeViewData *d, DWORD now) {
	if (d->anim_count <= 0) return false;
	float t = ( float ) (now - d->anim_start) / FLUX_TREE_ENTRANCE_MS;
	bool  running = t < 1.0f;
	if (t < 0.0f) t = 0.0f;
	if (!running) t = 1.0f;
	float p = flux_cubic_bezier(t, 0.0f, 0.0f, 0.0f, 1.0f);

	for (int i = 0; i < d->row_count; i++) {
		FluxNodeData *nd = flux_node_store_get(d->store, d->rows [i]);
		if (!nd) continue;
		int  flat              = d->win_first + i;
		bool entering          = flat >= d->anim_first && flat < d->anim_first + d->anim_count;
		nd->render_opacity     = entering ? p : 1.0f;
		nd->render_translate_y = entering ? (1.0f - p) * FLUX_TREE_ENTRANCE_DY : 0.0f;
	}
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

/* Stale watch (engine collect pass): scrolling left the realized window.
 * Node creation mid-traversal is unsafe, so realize on the next anim tick. */
static void tree_on_stale(void *ctx) {
	FluxTreeViewData *d = ( FluxTreeViewData * ) ctx;
	if (!d->window) { /* headless: no paint loop to defer around */
		tree_realize(d);
		return;
	}
	d->realize_pending = true;
	flux_anim_register(d, tree_step);
}

/* -------------------------------------------------------------------------
 * Interaction (TreeViewItem.cpp / TreeViewList keyboard)
 * ---------------------------------------------------------------------- */

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
		d->anim_start = GetTickCount();
		if (d->anim_count > 0) {
			flux_anim_register(d, tree_step);
			tree_tick_entrance(d, d->anim_start); /* first painted frame starts faded */
		}
	}
	if (d->on_expand) d->on_expand(d->userdata, flat, n->expanded);
	tree_repaint(d);
}

static void tree_select_single(FluxTreeViewData *d, int flat) {
	if (d->sel_mode != XTK_TREE_SELECT_SINGLE) return;
	int h = d->flat [flat];
	if (d->selected == h || d->nodes [h].disabled) return;
	d->selected = h;
	if (d->on_select) d->on_select(d->userdata, flat, true);
	tree_repaint(d);
}

/* Multiple: checkbox / Space toggles the node and cascades (a Partial parent
 * toggles to Selected, like a WinUI indeterminate CheckBox click). */
static void tree_toggle_check(FluxTreeViewData *d, int flat) {
	int  h   = d->flat [flat];
	bool now = d->nodes [h].sel_state != FLUX_TREE_SEL_SELECTED;
	tree_set_selected_multi(d, h, now);
	if (d->on_select) d->on_select(d->userdata, flat, now);
	tree_repaint(d);
}

/* Move keyboard focus to a flat row: scroll it into view, realize the new
 * window synchronously (input time — safe), then focus the row node. */
static void tree_focus_flat(FluxTreeViewData *d, int flat) {
	if (flat < 0) flat = 0;
	if (flat > d->flat_count - 1) flat = d->flat_count - 1;
	if (flat < 0) return;

	d->focused_flat = flat;
	flux_list_view_scroll_into_view(d->store, d->list, flat);
	tree_realize(d);

	XentNodeId row = tree_row_for_flat(d, flat);
	if (row != XENT_NODE_INVALID && d->input) flux_input_set_focus(d->input, row);
	tree_repaint(d);
}

static int tree_page_rows(FluxTreeViewData *d) {
	float offset = 0.0f, extent = 0.0f, cross = 0.0f;
	if (flux_list_view_viewport(d->store, d->list, &offset, &extent, &cross) && tree_pitch(d) > 0.0f) {
		int rows = ( int ) (extent / tree_pitch(d));
		if (rows > 0) return rows;
	}
	return 8;
}

/* Up/Down/Home/End/Page: flat moves; selection follows focus in Single mode
 * unless Ctrl is held (focus-only move, ListView behavior). */
static bool tree_key_nav(FluxTreeViewData *d, int flat, unsigned int vk) {
	int target = flat;
	switch (vk) {
	case VK_UP    : target = flat - 1; break;
	case VK_DOWN  : target = flat + 1; break;
	case VK_HOME  : target = 0; break;
	case VK_END   : target = d->flat_count - 1; break;
	case VK_PRIOR : target = flat - tree_page_rows(d); break;
	case VK_NEXT  : target = flat + tree_page_rows(d); break;
	default       : break;
	}
	if (target < 0) target = 0;
	if (target > d->flat_count - 1) target = d->flat_count - 1;
	if (target == flat && vk != VK_HOME && vk != VK_END) return true;
	if (!key_down(VK_CONTROL)) tree_select_single(d, target);
	tree_focus_flat(d, target);
	return true;
}

/* LTR Right: expand a collapsed parent, else hop to the first child. */
static bool tree_key_right(FluxTreeViewData *d, FluxTreeNode const *n, int flat) {
	if (n->first_child < 0) return false;
	if (!n->expanded) {
		tree_toggle_expand(d, flat);
		return true;
	}
	tree_select_single(d, flat + 1);
	tree_focus_flat(d, flat + 1);
	return true;
}

/* LTR Left: collapse an expanded parent, else hop to the parent row. */
static bool tree_key_left(FluxTreeViewData *d, FluxTreeNode const *n, int flat) {
	if (n->expanded && n->first_child >= 0) {
		tree_toggle_expand(d, flat);
		return true;
	}
	if (n->parent < 0) return false;
	int pf = tree_flat_of(d, n->parent);
	if (pf < 0) return true;
	tree_select_single(d, pf);
	tree_focus_flat(d, pf);
	return true;
}

static bool tree_key_check(FluxTreeViewData *d, FluxTreeNode const *n, int flat) {
	if (!n->disabled) tree_toggle_check(d, flat);
	return true;
}

static bool tree_key_invoke(FluxTreeViewData *d, FluxTreeNode const *n, int flat) {
	if (!n->disabled) {
		tree_select_single(d, flat);
		if (d->on_invoke) d->on_invoke(d->userdata, flat);
	}
	return true;
}

static bool tree_item_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxTreeItemData *it = ( FluxTreeItemData * ) ctx;
	FluxTreeViewData *d  = it->owner;
	int               flat = it->flat_index;
	if (!d || flat < 0 || flat >= d->flat_count) return false;
	FluxTreeNode const *n     = &d->nodes [d->flat [flat]];
	bool                plain = !key_down(VK_CONTROL) && !key_down(VK_SHIFT) && !key_down(VK_MENU);

	switch (vk) {
	case VK_UP :
	case VK_DOWN :
	case VK_HOME :
	case VK_END :
	case VK_PRIOR :
	case VK_NEXT  : return tree_key_nav(d, flat, vk);
	case VK_RIGHT : return plain && tree_key_right(d, n, flat); /* LTR: expand, else first child */
	case VK_LEFT  : return plain && tree_key_left(d, n, flat);  /* LTR: collapse, else parent */
	case VK_SPACE :
		if (tree_multi(d)) return tree_key_check(d, n, flat);
		/* fall through: Space invokes like Enter outside Multiple mode */
	case VK_RETURN : return tree_key_invoke(d, n, flat);
	default : return false;
	}
}

/* Chevron toggles on pointer *down* (OnExpandCollapseChevronPointerPressed,
 * handled so the row never selects/invokes); the checkbox arms here and
 * commits on release. Coordinates are node-local. */
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
		chevron = x >= ix + FLUX_TREE_MULTI_CHEVRON_X
		       && x < ix + FLUX_TREE_MULTI_CHEVRON_X + FLUX_TREE_CHEVRON_ZONE_MULTI;
	}
	else chevron = x >= ix && x < ix + FLUX_TREE_CHEVRON_ZONE;

	if (chevron && n->first_child >= 0) { /* leaf zone presses fall through to the row */
		it->press_on_chevron = true;
		tree_toggle_expand(d, flat);
	}
}

/* Row click (fires on release): invoke + select in Single; the checkbox zone
 * toggles the cascade; a chevron press was consumed on the way down. */
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
	tree_select_single(d, flat);
	if (d->on_invoke) d->on_invoke(d->userdata, flat);
}

/* -------------------------------------------------------------------------
 * Create / destroy
 * ---------------------------------------------------------------------- */

static void tree_destroy(void *component_data) {
	FluxTreeViewData *d = ( FluxTreeViewData * ) component_data;
	if (!d) return;
	flux_anim_unregister(d);
	for (int i = 0; i < d->node_count; i++) {
		if (!d->nodes [i].in_use) continue;
		flux_str_free(d->nodes [i].text);
		flux_str_free(d->nodes [i].icon_name);
	}
	/* Row nodes are subtree children — the store tears them down. */
	free(d->nodes);
	free(d->flat);
	free(d->rows);
	free(d);
}

XentNodeId flux_create_tree_view(FluxTreeViewCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_TREE_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_node_store_get(info->store, root);
	FluxTreeViewData *d  = nd ? ( FluxTreeViewData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	xent_set_protocol(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, root, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);

	/* The inner LIST spine supplies viewport + extent + the stale watch; the
	 * TreeView owns the rows, so the list gets no input/selection wiring. */
	XentNodeId list = flux_create_list_view(&(FluxListViewCreateInfo) {
	  .ctx = info->ctx, .store = info->store, .parent = root, .kind = XTK_LIST_KIND_LIST});
	if (list == XENT_NODE_INVALID) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}
	xent_set_flex_grow(info->ctx, list, 1.0f);
	xent_set_flex_basis(info->ctx, list, 0.0f);

	d->ctx          = info->ctx;
	d->store        = info->store;
	d->window       = info->window;
	d->input        = info->input;
	d->root         = root;
	d->list         = list;
	d->host         = flux_list_view_content_node(info->store, list);
	d->free_head    = -1;
	d->first_root   = -1;
	d->sel_mode     = info->selection_mode;
	d->selected     = -1;
	d->row_height   = info->row_height > 0.0f ? info->row_height : FLUX_TREE_ROW_H;
	d->focused_flat = -1;
	d->on_invoke    = info->on_invoke;
	d->on_expand    = info->on_expand;
	d->on_select    = info->on_select;
	d->userdata     = info->userdata;

	nd->component_data         = d;
	nd->destroy_component_data = tree_destroy;

	flux_list_view_set_stale_callback(info->store, list, tree_on_stale, d);
	xent_set_semantic_role(info->ctx, root, XENT_SEMANTIC_CONTAINER);
	return root;
}

/* -------------------------------------------------------------------------
 * Store-facing model mutators (programmatic — no callbacks fire)
 * ---------------------------------------------------------------------- */

int flux_tree_view_add_node(FluxNodeStore *store, XentNodeId tree, int parent, char const *text, char const *icon) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d) return -1;
	if (parent >= 0 && !tree_valid(d, parent)) return -1;
	int h = tree_node_alloc(d);
	if (h < 0) return -1;

	FluxTreeNode  *n = &d->nodes [h];
	n->parent        = parent;
	n->text          = flux_str_dup(text);
	n->icon_name     = flux_str_dup(icon);
	wchar_t const *g = flux_icon_lookup(icon);
	n->glyph         = g && g [0] ? ( uint32_t ) g [0] : 0;
	n->depth         = parent >= 0 ? ( int16_t ) (d->nodes [parent].depth + 1) : 0;
	/* A new child of a selected parent inherits its state (ViewModel). */
	if (tree_multi(d) && parent >= 0 && d->nodes [parent].sel_state == FLUX_TREE_SEL_SELECTED)
		n->sel_state = FLUX_TREE_SEL_SELECTED;

	int *link = parent >= 0 ? &d->nodes [parent].first_child : &d->first_root;
	while (*link >= 0) link = &d->nodes [*link].next_sibling;
	*link = h;

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

static void tree_unlink(FluxTreeViewData *d, int h) {
	int  parent = d->nodes [h].parent;
	int *link   = parent >= 0 ? &d->nodes [parent].first_child : &d->first_root;
	while (*link >= 0 && *link != h) link = &d->nodes [*link].next_sibling;
	if (*link == h) *link = d->nodes [h].next_sibling;
}

void flux_tree_view_remove_node(FluxNodeStore *store, XentNodeId tree, int node) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || !tree_valid(d, node)) return;
	int parent = d->nodes [node].parent;
	tree_unlink(d, node);
	tree_free_subtree(d, node);
	/* Removal can flip an ancestor between Partial/Selected/UnSelected. */
	if (tree_multi(d) && parent >= 0 && d->nodes [parent].first_child >= 0) {
		d->nodes [parent].sel_state = tree_derive_state(d, parent);
		tree_update_ancestors(d, parent);
	}
	tree_sync_flat(d);
	tree_repaint(d);
}

void flux_tree_view_clear(FluxNodeStore *store, XentNodeId tree) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d) return;
	while (d->first_root >= 0) {
		int r         = d->first_root;
		d->first_root = d->nodes [r].next_sibling;
		tree_free_subtree(d, r);
	}
	d->selected = -1;
	tree_sync_flat(d);
	tree_repaint(d);
}

void flux_tree_view_set_expanded(FluxNodeStore *store, XentNodeId tree, int node, bool expanded) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || !tree_valid(d, node) || d->nodes [node].expanded == expanded) return;
	d->nodes [node].expanded = expanded;
	tree_sync_flat(d);
	tree_repaint(d);
}

void flux_tree_view_set_node_disabled(FluxNodeStore *store, XentNodeId tree, int node, bool disabled) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || !tree_valid(d, node)) return;
	d->nodes [node].disabled = disabled;
	tree_realize(d); /* refresh row semantics */
	tree_repaint(d);
}

void flux_tree_view_set_selection_mode(FluxNodeStore *store, XentNodeId tree, XtkTreeSelMode mode) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || d->sel_mode == mode) return;
	d->sel_mode = mode;
	d->selected = -1; /* switching modes clears the selection (WinUI) */
	for (int i = 0; i < d->node_count; i++)
		if (d->nodes [i].in_use) d->nodes [i].sel_state = FLUX_TREE_SEL_NONE;
	tree_repaint(d);
}

void flux_tree_view_set_selected(FluxNodeStore *store, XentNodeId tree, int node, bool selected) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || !tree_valid(d, node)) return;
	if (d->sel_mode == XTK_TREE_SELECT_SINGLE) d->selected = selected ? node : (d->selected == node ? -1 : d->selected);
	else if (tree_multi(d)) tree_set_selected_multi(d, node, selected);
	tree_repaint(d);
}

void flux_tree_view_select_all(FluxNodeStore *store, XentNodeId tree) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || !tree_multi(d)) return; /* Multiple mode only (WinUI SelectAll) */
	for (int r = d->first_root; r >= 0; r = d->nodes [r].next_sibling) tree_cascade_down(d, r, FLUX_TREE_SEL_SELECTED);
	tree_repaint(d);
}

/* -------------------------------------------------------------------------
 * Queries (gallery / tests)
 * ---------------------------------------------------------------------- */

bool flux_tree_view_is_expanded(FluxNodeStore *store, XentNodeId tree, int node) {
	FluxTreeViewData *d = tree_data(store, tree);
	return d && tree_valid(d, node) && d->nodes [node].expanded;
}

bool flux_tree_view_is_selected(FluxNodeStore *store, XentNodeId tree, int node) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || !tree_valid(d, node)) return false;
	if (d->sel_mode == XTK_TREE_SELECT_SINGLE) return d->selected == node;
	return d->nodes [node].sel_state == FLUX_TREE_SEL_SELECTED;
}

int flux_tree_view_flat_count(FluxNodeStore *store, XentNodeId tree) {
	FluxTreeViewData *d = tree_data(store, tree);
	return d ? d->flat_count : 0;
}

int flux_tree_view_flat_node(FluxNodeStore *store, XentNodeId tree, int flat_index) {
	FluxTreeViewData *d = tree_data(store, tree);
	if (!d || flat_index < 0 || flat_index >= d->flat_count) return -1;
	return d->flat [flat_index];
}
