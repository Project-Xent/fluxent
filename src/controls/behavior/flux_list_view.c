/**
 * @file flux_list_view.c
 * @brief Virtualized items-host family (ListView / ListBox / GridView /
 * ItemsRepeater): retained spine, recycled cells, the per-frame staleness
 * watch, and the WinUI ListViewBase selection + keyboard state machine.
 *
 * Cell realization lives in the xtk reconciler (xtk_list_realize); this
 * file owns the retained node structure, scroll integration, and the
 * selection model. Multiple/Extended selection is control-retained as
 * sorted merged ranges (WinUI keeps it in the Selector too — an app model
 * can't hold Ctrl/Shift anchors); Single mode stays app-controlled.
 *
 * Interaction semantics follow ListViewBase_Partial_Interaction.cpp:
 * primary gesture = click/Space/Enter, secondary = Ctrl+that; plain arrows
 * collapse to single selection (unless Ctrl: focus-only move); Shift+arrow
 * extends from the anchor (Ctrl+Shift keeps other ranges in Extended);
 * Ctrl+A toggles select-all in Multiple/Extended; Escape does not clear.
 */
#include "controls/factory/flux_factory.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_input.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static FluxListViewData *list_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || !nd->component_data) return NULL;
	if (nd->component_type != FLUX_CONTROL_LIST && nd->component_type != FLUX_CONTROL_LIST_BOX
	    && nd->component_type != FLUX_CONTROL_GRID_VIEW && nd->component_type != FLUX_CONTROL_ITEMS_REPEATER
	    && nd->component_type != FLUX_CONTROL_ITEMS_VIEW)
		return NULL;
	return ( FluxListViewData * ) nd->component_data;
}

static FluxListItemData *item_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_LIST_ITEM || !nd->component_data) return NULL;
	return ( FluxListItemData * ) nd->component_data;
}

static FluxControlType list_type_for_kind(XtkListKind kind) {
	switch (kind) {
	case XTK_LIST_KIND_LIST_BOX : return FLUX_CONTROL_LIST_BOX;
	case XTK_LIST_KIND_GRID     : return FLUX_CONTROL_GRID_VIEW;
	case XTK_LIST_KIND_REPEATER : return FLUX_CONTROL_ITEMS_REPEATER;
	default                     : return FLUX_CONTROL_LIST;
	}
}

/* -------------------------------------------------------------------------
 * Selection ranges (sorted, inclusive, merged)
 * ---------------------------------------------------------------------- */

static void ranges_clear(FluxListViewData *ld) { ld->range_count = 0; }

static bool ranges_reserve(FluxListViewData *ld, int count) {
	if (count <= ld->range_cap) return true;
	int cap = ld->range_cap ? ld->range_cap * 2 : 8;
	if (cap < count) cap = count;
	FluxListSelRange *r = ( FluxListSelRange * ) realloc(ld->ranges, sizeof(*r) * ( size_t ) cap);
	if (!r) return false;
	ld->ranges    = r;
	ld->range_cap = cap;
	return true;
}

static bool ranges_contains(FluxListViewData const *ld, int index) {
	for (int i = 0; i < ld->range_count; i++)
		if (index >= ld->ranges [i].first && index <= ld->ranges [i].last) return true;
	return false;
}

/* Insert [first,last] and merge overlapping/adjacent runs (list stays sorted). */
static void ranges_add(FluxListViewData *ld, int first, int last) {
	if (!ranges_reserve(ld, ld->range_count + 1)) return;

	int pos = 0;
	while (pos < ld->range_count && ld->ranges [pos].last < first - 1) pos++;

	int merged_first = first, merged_last = last;
	int span = 0;
	while (pos + span < ld->range_count && ld->ranges [pos + span].first <= last + 1) {
		if (ld->ranges [pos + span].first < merged_first) merged_first = ld->ranges [pos + span].first;
		if (ld->ranges [pos + span].last > merged_last) merged_last = ld->ranges [pos + span].last;
		span++;
	}

	memmove(&ld->ranges [pos + 1], &ld->ranges [pos + span], sizeof(*ld->ranges) * ( size_t ) (ld->range_count - pos - span));
	ld->ranges [pos]  = (FluxListSelRange) {merged_first, merged_last};
	ld->range_count  += 1 - span;
}

/* Split the run at slot i in two around index (which sits strictly inside). */
static void ranges_split(FluxListViewData *ld, int i, int index) {
	if (!ranges_reserve(ld, ld->range_count + 1)) return;
	memmove(&ld->ranges [i + 1], &ld->ranges [i], sizeof(*ld->ranges) * ( size_t ) (ld->range_count - i));
	ld->ranges [i].last      = index - 1;
	ld->ranges [i + 1].first = index + 1;
	ld->range_count++;
}

/* Remove index from the set, splitting its run when it sits inside one. */
static void ranges_remove(FluxListViewData *ld, int index) {
	int i = 0;
	while (i < ld->range_count && (index < ld->ranges [i].first || index > ld->ranges [i].last)) i++;
	if (i == ld->range_count) return;

	FluxListSelRange *r = &ld->ranges [i];
	if (r->first == r->last) {
		memmove(&ld->ranges [i], &ld->ranges [i + 1], sizeof(*r) * ( size_t ) (ld->range_count - i - 1));
		ld->range_count--;
		return;
	}
	if (index == r->first) { r->first++; return; }
	if (index == r->last) { r->last--; return; }
	ranges_split(ld, i, index);
}

static int ranges_total(FluxListViewData const *ld) {
	int n = 0;
	for (int i = 0; i < ld->range_count; i++) n += ld->ranges [i].last - ld->ranges [i].first + 1;
	return n;
}

/* -------------------------------------------------------------------------
 * Selection state machine (ListViewBase_Partial_Selection/Interaction)
 * ---------------------------------------------------------------------- */

static bool multi_mode(FluxListViewData const *ld) {
	return ld->sel_mode == XTK_LIST_SELECT_MULTIPLE || ld->sel_mode == XTK_LIST_SELECT_EXTENDED;
}

static void list_report(FluxListViewData *ld, int lead) {
	if (ld->on_select) ld->on_select(ld->on_select_ctx, lead);
}

static void list_invoke(FluxListViewData *ld, int index) {
	if (ld->on_invoke && ld->item_invoked_enabled && ld->sel_mode == XTK_LIST_SELECT_NONE)
		ld->on_invoke(ld->on_invoke_ctx, index);
}

/* MakeSingleSelection: replace the set with one item; anchor follows in
 * Extended/Multiple only. */
static void sel_single(FluxListViewData *ld, int index) {
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) {
		list_report(ld, index); /* app-controlled: report intent only */
		return;
	}
	ranges_clear(ld);
	ranges_add(ld, index, index);
	ld->anchor     = index;
	ld->anchor_set = true;
	list_report(ld, index);
}

/* MakeToggleSelection. */
static void sel_toggle(FluxListViewData *ld, int index) {
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) {
		/* WinUI Ctrl+click in Single toggles the one selection. */
		list_report(ld, ld->selected == index ? -1 : index);
		return;
	}
	if (ranges_contains(ld, index)) ranges_remove(ld, index);
	else ranges_add(ld, index, index);
	ld->anchor     = index;
	ld->anchor_set = true;
	list_report(ld, index);
}

/* MakeRangeSelection: anchor..index; clear_old deselects everything else.
 * A missing anchor falls back to the current lead (WinUI: SelectedIndex). */
static void sel_range(FluxListViewData *ld, int index, bool clear_old) {
	if (!multi_mode(ld)) {
		sel_single(ld, index);
		return;
	}
	int anchor = ld->anchor_set ? ld->anchor : (ld->range_count ? ld->ranges [0].first : 0);
	if (!ld->anchor_set) {
		ld->anchor     = anchor;
		ld->anchor_set = true;
	}
	int lo = anchor < index ? anchor : index;
	int hi = anchor < index ? index : anchor;
	if (clear_old) ranges_clear(ld);
	ranges_add(ld, lo, hi);
	list_report(ld, index);
}

/* Ctrl+A: toggle select-all (Multiple/Extended only). */
static void sel_all_toggle(FluxListViewData *ld) {
	if (!multi_mode(ld) || ld->count <= 0) return;
	bool all = ld->range_count == 1 && ld->ranges [0].first == 0 && ld->ranges [0].last == ld->count - 1;
	ranges_clear(ld);
	if (!all) ranges_add(ld, 0, ld->count - 1);
	list_report(ld, all ? -1 : ld->count - 1);
}

static bool key_down(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }

/* Primary gesture (click / Space / Enter) — Interaction.cpp OnSelectItemPrimary. */
static void list_gesture_primary(FluxListViewData *ld, int index) {
	bool shift = key_down(VK_SHIFT);
	switch (ld->sel_mode) {
	case XTK_LIST_SELECT_NONE     : list_report(ld, index); break; /* ItemClick-style */
	case XTK_LIST_SELECT_SINGLE   : sel_single(ld, index); break;
	case XTK_LIST_SELECT_MULTIPLE : shift ? sel_range(ld, index, false) : sel_toggle(ld, index); break;
	case XTK_LIST_SELECT_EXTENDED : shift ? sel_range(ld, index, true) : sel_single(ld, index); break;
	}
}

/* Secondary gesture (Ctrl+click / Ctrl+Space) — OnSelectItemSecondary. */
static void list_gesture_secondary(FluxListViewData *ld, int index) {
	bool shift = key_down(VK_SHIFT);
	switch (ld->sel_mode) {
	case XTK_LIST_SELECT_NONE     : list_report(ld, index); break;
	case XTK_LIST_SELECT_SINGLE   : sel_toggle(ld, index); break;
	case XTK_LIST_SELECT_MULTIPLE : shift ? sel_range(ld, index, false) : sel_toggle(ld, index); break;
	case XTK_LIST_SELECT_EXTENDED : shift ? sel_range(ld, index, false) : sel_toggle(ld, index); break;
	}
}

/* -------------------------------------------------------------------------
 * Keyboard navigation + scroll-into-view
 * ---------------------------------------------------------------------- */

static bool list_viewport_metrics(FluxListViewData const *ld, float *offset, float *extent) {
	FluxNodeData *snd  = flux_node_store_get(ld->store, ld->scroll);
	XentRect      rect = {0};
	if (!snd || !snd->component_data || !xent_get_layout_rect(ld->ctx, ld->scroll, &rect) || rect.h <= 0.0f)
		return false;
	*offset = (( FluxScrollData * ) snd->component_data)->scroll_y;
	*extent = rect.h;
	return true;
}

/* Minimal-edge alignment (ScrollIntoViewAlignment_Default): scroll just far
 * enough that the cell's row is fully inside the viewport. */
void flux_list_view_scroll_into_view(FluxNodeStore *store, XentNodeId list, int index) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld || index < 0 || index >= ld->count) return;
	float offset = 0.0f, extent = 0.0f;
	if (!list_viewport_metrics(ld, &offset, &extent)) return;

	int   cols  = ld->cols > 0 ? ld->cols : 1;
	float y     = ( float ) (index / cols) * ld->item_height;
	float new_y = offset;
	if (y < offset) new_y = y;
	else if (y + ld->item_height > offset + extent) new_y = y + ld->item_height - extent;
	if (new_y != offset) flux_scroll_set_offset(store, ld->scroll, 0.0f, new_y);
}

static XentNodeId list_find_item_node(FluxListViewData const *ld, int index) {
	for (XentNodeId child = xent_get_first_child(ld->ctx, ld->host); child != XENT_NODE_INVALID;
	  child               = xent_get_next_sibling(ld->ctx, child))
	{
		FluxNodeData *nd = ( FluxNodeData * ) xent_get_userdata(ld->ctx, child);
		if (!nd || nd->component_type != FLUX_CONTROL_LIST_ITEM || !nd->component_data) continue;
		if ((( FluxListItemData * ) nd->component_data)->index == index) return child;
	}
	return XENT_NODE_INVALID;
}

/* Move keyboard focus to an item index: scroll it into view first; if its
 * node isn't realized yet, park it in pending_focus — the placement setter
 * completes the move once the reconciler realizes the window. */
static void list_focus_index(FluxListViewData *ld, int index) {
	if (index < 0) index = 0;
	if (index > ld->count - 1) index = ld->count - 1;
	if (index < 0) return;

	ld->focused = index;
	flux_list_view_scroll_into_view(ld->store, ld->root, index);

	XentNodeId node = list_find_item_node(ld, index);
	if (node != XENT_NODE_INVALID) {
		if (ld->input) flux_input_set_focus(ld->input, node);
		ld->pending_focus = -1;
	}
	else ld->pending_focus = index;
}

/* Arrow/Home/End/Page target — GridView 2D nav: Left/Right ±1, Up/Down ±cols. */
static int list_nav_target(FluxListViewData const *ld, int from, int vk) {
	int   cols   = ld->cols > 0 ? ld->cols : 1;
	float extent = 0.0f, offset = 0.0f;
	int   page_rows = 8;
	if (list_viewport_metrics(ld, &offset, &extent) && ld->item_height > 0.0f)
		page_rows = ( int ) (extent / ld->item_height);
	if (page_rows < 1) page_rows = 1;

	switch (vk) {
	case VK_UP    : return from - cols;
	case VK_DOWN  : return from + cols;
	case VK_LEFT  : return cols > 1 ? from - 1 : from;
	case VK_RIGHT : return cols > 1 ? from + 1 : from;
	case VK_HOME  : return 0;
	case VK_END   : return ld->count - 1;
	case VK_PRIOR : return from - page_rows * cols;
	case VK_NEXT  : return from + page_rows * cols;
	default       : return from;
	}
}

/* Selection-follows-focus rules — Interaction.cpp OnKeyboardNavigation. */
static void list_nav_select(FluxListViewData *ld, int index) {
	bool ctrl  = key_down(VK_CONTROL);
	bool shift = key_down(VK_SHIFT);

	if (ld->sel_mode == XTK_LIST_SELECT_EXTENDED && shift) {
		sel_range(ld, index, !ctrl);
		return;
	}
	if (ld->sel_mode == XTK_LIST_SELECT_MULTIPLE) {
		if (shift) sel_range(ld, index, false);
		ld->anchor     = index; /* Multiple: anchor always follows focus */
		ld->anchor_set = true;
		return;
	}
	if (ctrl) return; /* focus-only move */
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE && ld->single_follows_focus) sel_single(ld, index);
	if (ld->sel_mode == XTK_LIST_SELECT_EXTENDED) sel_single(ld, index);
}

/* Arrow/Home/End/Page: clamp the target, move selection per mode, focus it.
 * A no-op move still swallows the key (except Home/End, which always land). */
static bool list_key_navigate(FluxListViewData *ld, FluxListItemData const *it, unsigned int vk) {
	int target = list_nav_target(ld, it->index, ( int ) vk);
	if (target < 0) target = 0;
	if (target > ld->count - 1) target = ld->count - 1;
	if (target == it->index && vk != VK_HOME && vk != VK_END) return true;
	list_nav_select(ld, target);
	list_focus_index(ld, target);
	return true;
}

static bool list_item_on_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxListItemData *it = ( FluxListItemData * ) ctx;
	FluxListViewData *ld = it->owner;
	if (!ld || ld->kind == XTK_LIST_KIND_REPEATER || ld->count <= 0) return false;

	switch (vk) {
	case VK_UP :
	case VK_DOWN :
	case VK_LEFT :
	case VK_RIGHT :
	case VK_HOME :
	case VK_END :
	case VK_PRIOR :
	case VK_NEXT : return list_key_navigate(ld, it, vk);
	case VK_SPACE :
		if (key_down(VK_CONTROL)) list_gesture_secondary(ld, it->index);
		else list_gesture_primary(ld, it->index);
		list_invoke(ld, it->index);
		return true;
	case VK_RETURN : list_gesture_primary(ld, it->index); list_invoke(ld, it->index); return true;
	case 'A' :
		if (!key_down(VK_CONTROL) || key_down(VK_SHIFT) || !multi_mode(ld)) return false;
		sel_all_toggle(ld);
		return true;
	default : return false;
	}
}

/* Click (fires on release) routes through the same gesture machinery so
 * Ctrl/Shift+click follow WinUI exactly. */
static void list_item_on_click(void *ctx) {
	FluxListItemData *it = ( FluxListItemData * ) ctx;
	FluxListViewData *ld = it->owner;
	if (!ld || ld->kind == XTK_LIST_KIND_REPEATER) return;
	ld->focused = it->index;
	if (key_down(VK_CONTROL)) list_gesture_secondary(ld, it->index);
	else list_gesture_primary(ld, it->index);
	list_invoke(ld, it->index);
}

/* -------------------------------------------------------------------------
 * Create / destroy
 * ---------------------------------------------------------------------- */

static void list_view_data_destroy(void *component_data) {
	FluxListViewData *ld = ( FluxListViewData * ) component_data;
	if (!ld) return;
	free(ld->ranges);
	free(ld);
}

XentNodeId flux_create_list_view(FluxListViewCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	FluxControlType type = list_type_for_kind(info->kind);
	XentNodeId      root = flux_factory_create_node(info->ctx, info->store, info->parent, type);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_node_store_get(info->store, root);
	FluxListViewData *ld = nd ? ( FluxListViewData * ) calloc(1, sizeof(FluxListViewData)) : NULL;
	if (!nd || !ld) {
		free(ld);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	xent_set_protocol(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, root, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);

	XentNodeId scroll = flux_create_scroll(&(FluxContainerCreateInfo) {info->ctx, info->store, root});
	XentNodeId host   = scroll != XENT_NODE_INVALID
	    ? flux_factory_create_node(info->ctx, info->store, scroll, FLUX_CONTROL_CONTAINER)
	    : XENT_NODE_INVALID;
	if (host == XENT_NODE_INVALID) {
		free(ld);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	/* The scroll viewport fills the list; the host is the scrolled content:
	 * absolutely-positioned recycled cells over a small rebased canvas. The
	 * logical extent is pushed manually onto the scroll (list_apply_host_extent);
	 * the host must never flex-shrink to the viewport. */
	xent_set_flex_grow(info->ctx, scroll, 1.0f);
	xent_set_flex_basis(info->ctx, scroll, 0.0f);
	xent_set_protocol(info->ctx, host, XENT_PROTOCOL_ABSOLUTE);
	xent_set_width_percent(info->ctx, host, 1.0f);
	xent_set_height(info->ctx, host, 0.0f);
	xent_set_flex_shrink(info->ctx, host, 0.0f);

	ld->ctx                  = info->ctx;
	ld->store                = info->store;
	ld->input                = info->input;
	ld->root                 = root;
	ld->scroll               = scroll;
	ld->host                 = host;
	ld->kind                 = info->kind;
	ld->cols                 = 1;
	ld->selected             = -1;
	ld->realized_last        = -1;
	ld->focused              = -1;
	ld->pending_focus        = -1;
	ld->single_follows_focus = true;
	ld->on_select            = info->on_select;
	ld->on_select_ctx        = info->userdata;

	nd->component_data         = ld;
	nd->destroy_component_data = list_view_data_destroy;

	xent_set_semantic_role(info->ctx, root, XENT_SEMANTIC_CONTAINER);
	return root;
}

XentNodeId flux_create_list_item(FluxListItemCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_LIST_ITEM);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_node_store_get(info->store, node);
	FluxListItemData *it = nd ? ( FluxListItemData * ) calloc(1, sizeof(FluxListItemData)) : NULL;
	if (!nd || !it) {
		free(it);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	/* Owner = LIST root two levels up (parent is the host inside the scroll). */
	XentNodeId scroll = xent_get_parent(info->ctx, info->parent);
	XentNodeId root   = scroll != XENT_NODE_INVALID ? xent_get_parent(info->ctx, scroll) : XENT_NODE_INVALID;
	it->owner         = root != XENT_NODE_INVALID ? list_data(info->store, root) : NULL;
	it->index         = -1;

	nd->component_data         = it;
	nd->destroy_component_data = free;

	bool interactive = !it->owner || it->owner->kind != XTK_LIST_KIND_REPEATER;
	if (interactive) {
		nd->behavior.on_click     = list_item_on_click;
		nd->behavior.on_click_ctx = it;
		nd->behavior.on_key       = list_item_on_key;
		nd->behavior.on_key_ctx   = it;
		xent_set_focusable(info->ctx, node, true);
		xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);
	}

	/* WinUI ListViewItem: content inset from the selection chrome, centered
	 * on the row axis. GridViewItem centers both axes. */
	xent_set_protocol(info->ctx, node, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, node, XENT_FLEX_COLUMN);
	xent_set_flex_justify_content(info->ctx, node, XENT_FLEX_JUSTIFY_CENTER);
	if (it->owner && it->owner->kind == XTK_LIST_KIND_GRID) {
		xent_set_flex_align_items(info->ctx, node, XENT_FLEX_ALIGN_CENTER);
		xent_set_padding(info->ctx, node, (XentInsets) {0.0f, 0.0f, 4.0f, 4.0f}); /* GridViewItem margin 0,0,4,4 */
	}
	else if (it->owner && it->owner->kind == XTK_LIST_KIND_LIST_BOX) {
		xent_set_flex_align_items(info->ctx, node, XENT_FLEX_ALIGN_STRETCH);
		xent_set_padding(info->ctx, node, (XentInsets) {12.0f, 9.0f, 12.0f, 12.0f}); /* ListBoxItemPadding */
	}
	else {
		xent_set_flex_align_items(info->ctx, node, XENT_FLEX_ALIGN_STRETCH);
		xent_set_padding(info->ctx, node, (XentInsets) {16.0f, 0.0f, 12.0f, 0.0f}); /* ListViewItem 16,0,12,0 */
	}
	if (!it->owner || it->owner->item_width <= 0.0f) xent_set_width_percent(info->ctx, node, 1.0f);

	return node;
}

/* -------------------------------------------------------------------------
 * Store-facing setters / accessors
 * ---------------------------------------------------------------------- */

XentNodeId flux_list_view_content_node(FluxNodeStore *store, XentNodeId list) {
	FluxListViewData *ld = list_data(store, list);
	return ld ? ld->host : XENT_NODE_INVALID;
}

static FluxScrollData *list_scroll_data(FluxListViewData const *ld) {
	FluxNodeData *snd = flux_node_store_get(ld->store, ld->scroll);
	return snd ? ( FluxScrollData * ) snd->component_data : NULL;
}

/* Virtual extent: the rows × item_height content exists only as the scroll's
 * manual logical extent (scrollbar + clamping). The host stays a small
 * physical canvas spanning just the realized window (set_realized), so the
 * compositor / hit paths never see deep-scroll coordinates. */
static void list_apply_host_extent(FluxListViewData *ld) {
	int cols = ld->cols > 0 ? ld->cols : 1;
	int rows = cols > 0 ? (ld->count + cols - 1) / cols : ld->count;
	FluxScrollData *sd = list_scroll_data(ld);
	if (!sd) return;
	sd->content_manual = 1;
	sd->virtualized    = 1;
	sd->content_w      = 0.0f;
	sd->content_h      = ( float ) rows * ld->item_height;
}

void flux_list_view_set_extent(
  FluxNodeStore *store, XentNodeId list, int count, float item_height, float item_width, int columns
) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return;
	if (count < 0) count = 0;
	if (item_height <= 0.0f) item_height = 40.0f;
	if (ld->count == count && ld->item_height == item_height && ld->item_width == item_width
	    && ld->columns == columns)
		return;
	ld->count       = count;
	ld->item_height = item_height;
	ld->item_width  = item_width;
	ld->columns     = columns;
	list_apply_host_extent(ld);
}

void flux_list_view_set_sel_mode(FluxNodeStore *store, XentNodeId list, XtkListSelMode mode) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld || ld->sel_mode == mode) return;
	ld->sel_mode   = mode;
	ld->anchor_set = false;
	ranges_clear(ld);
}

void flux_list_view_set_selected(FluxNodeStore *store, XentNodeId list, int selected) {
	FluxListViewData *ld = list_data(store, list);
	if (ld) ld->selected = selected;
}

bool flux_list_view_is_selected(FluxNodeStore *store, XentNodeId list, int index) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return false;
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) return index == ld->selected;
	return ranges_contains(ld, index);
}

int flux_list_view_selection_count(FluxNodeStore *store, XentNodeId list) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return 0;
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) return ld->selected >= 0 ? 1 : 0;
	return ranges_total(ld);
}

void flux_list_view_set_realized(FluxNodeStore *store, XentNodeId list, int first, int last, int cols) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return;
	ld->realized_first = first;
	ld->realized_last  = last;
	ld->stale_fired    = false;
	if (cols > 0 && cols != ld->cols) {
		ld->cols = cols;
		list_apply_host_extent(ld);
	}

	/* Rebase the physical window: origin = first realized row's logical y, host
	 * spans just the realized rows. Retained cells that kept their logical slot
	 * are re-anchored (the reconciler only re-places changed ones), so physical
	 * coordinates never grow with scroll depth. */
	int   c         = ld->cols > 0 ? ld->cols : 1;
	int   row_first = first / c;
	float span      = last >= first ? ( float ) (last / c - row_first + 1) * ld->item_height : 0.0f;
	float origin    = ( float ) row_first * ld->item_height;
	xent_set_height(ld->ctx, ld->host, span);

	FluxScrollData *sd = list_scroll_data(ld);
	if (!sd || sd->origin_y == origin) return;
	sd->origin_y = origin;
	for (XentNodeId child = xent_get_first_child(ld->ctx, ld->host); child != XENT_NODE_INVALID;
	  child               = xent_get_next_sibling(ld->ctx, child))
	{
		FluxListItemData *it = item_data(store, child);
		if (it && it->index >= 0)
			xent_set_absolute_position(
			  ld->ctx, child, (XentPoint) {it->logical_x - sd->origin_x, it->logical_y - origin}
			);
	}
}

void flux_list_view_set_stale_callback(FluxNodeStore *store, XentNodeId list, void (*on_stale)(void *), void *ctx) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return;
	ld->on_stale     = on_stale;
	ld->on_stale_ctx = ctx;
}

bool flux_list_view_viewport(FluxNodeStore *store, XentNodeId list, float *offset, float *extent, float *cross) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return false;
	XentRect rect = {0};
	if (!list_viewport_metrics(ld, offset, extent)) return false;
	*cross = xent_get_layout_rect(ld->ctx, ld->scroll, &rect) ? rect.w : 0.0f;
	return true;
}

/* Fired from the engine's collect pass each rendered frame: while scrolling,
 * frames are already being produced, so this is exactly the cadence at which
 * the realized window can go stale. Fires once per window (set_realized
 * re-arms). Grid resizes count too: a column-count change invalidates every
 * cell position, so it re-realizes even when the index range still covers. */
void flux_list_view_update_window(XentContext *ctx, XentNodeId list, struct FluxNodeData *nd) {
	( void ) list;
	FluxListViewData *ld = ( FluxListViewData * ) nd->component_data;
	if (!ld || !ld->on_stale || ld->stale_fired || ld->count <= 0 || ld->item_height <= 0.0f) return;

	FluxNodeData *snd  = ( FluxNodeData * ) xent_get_userdata(ctx, ld->scroll);
	XentRect      rect = {0};
	if (!snd || !snd->component_data || !xent_get_layout_rect(ctx, ld->scroll, &rect) || rect.h <= 0.0f) return;
	float offset = (( FluxScrollData * ) snd->component_data)->scroll_y;

	int  cols       = xtk_list_auto_columns(rect.w, ld->item_width, ld->columns);
	int  total_rows = (ld->count + cols - 1) / cols;
	int  row_first  = ( int ) floorf(offset / ld->item_height);
	int  row_last   = ( int ) ceilf((offset + rect.h) / ld->item_height) - 1;
	if (row_first < 0) row_first = 0;
	if (row_last > total_rows - 1) row_last = total_rows - 1;

	int visible_first = row_first * cols;
	int visible_last  = (row_last + 1) * cols - 1;
	if (visible_last > ld->count - 1) visible_last = ld->count - 1;

	if (cols != ld->cols || visible_first < ld->realized_first || visible_last > ld->realized_last) {
		ld->stale_fired = true;
		ld->on_stale(ld->on_stale_ctx);
	}
}

void flux_list_item_set_place(FluxNodeStore *store, XentNodeId item, int index, float x, float y) {
	FluxListItemData *it = item_data(store, item);
	if (!it) return;
	it->index     = index;
	it->logical_x = x;
	it->logical_y = y;
	FluxScrollData *sd = it->owner ? list_scroll_data(it->owner) : NULL;
	xent_set_absolute_position(
	  flux_node_store_context(store), item,
	  (XentPoint) {x - (sd ? sd->origin_x : 0.0f), y - (sd ? sd->origin_y : 0.0f)}
	);

	/* Complete a keyboard focus move that targeted a not-yet-realized cell. */
	FluxListViewData *ld = it->owner;
	if (ld && ld->pending_focus == index) {
		ld->pending_focus = -1;
		if (ld->input) flux_input_set_focus(ld->input, item);
	}
}

void flux_list_item_set_state(FluxNodeStore *store, XentNodeId item, bool selected, bool multi) {
	FluxListItemData *it = item_data(store, item);
	if (!it) return;
	it->selected = selected;
	if (it->multi != multi) {
		it->multi = multi;
		/* Multiple mode reserves the 20px checkbox + margins on the leading
		 * edge (rounded content offset 28). Lists only — grid overlays. */
		if (it->owner && it->owner->kind == XTK_LIST_KIND_LIST) {
			XentInsets pad = multi ? (XentInsets) {16.0f + 28.0f, 0.0f, 12.0f, 0.0f}
			                       : (XentInsets) {16.0f, 0.0f, 12.0f, 0.0f};
			xent_set_padding(flux_node_store_context(store), item, pad);
		}
	}
}
