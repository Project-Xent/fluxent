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
#include "store/flux_node_data.h"
#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_items.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_input.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <xtk/xtk_catalog.h>

static FluxListViewData *list_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || !nd->component_data) return NULL;
	if (nd->component_type != FLUX_CONTROL_LIST
		&& nd->component_type != FLUX_CONTROL_LIST_BOX
		&& nd->component_type != FLUX_CONTROL_GRID_VIEW
		&& nd->component_type != FLUX_CONTROL_ITEMS_REPEATER
		&& nd->component_type != FLUX_CONTROL_ITEMS_VIEW)
		return NULL;
	return ( FluxListViewData * ) nd->component_data;
}

static FluxListItemData *item_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
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

	memmove(
	  &ld->ranges [pos + 1], &ld->ranges [pos + span], sizeof(*ld->ranges) * ( size_t ) (ld->range_count - pos - span)
	);
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
	if (index == r->first) {
		r->first++;
		return;
	}
	if (index == r->last) {
		r->last--;
		return;
	}
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
	void     (*notify)(void *, int) = ld->on_select;
	void    *notify_ctx             = ld->on_select_ctx;
	XtkHost *host                   = flux_store_host(ld->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_SELECT,
		  .target         = ( XtkCtrl ) ld->root,
		  .source         = ( XtkCtrl ) ld->root,
		  .current_target = ( XtkCtrl ) ld->root,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = XTK_EVENT_CAUSE_USER,
		  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = lead},
		};
		flux_host_route(host, &event);
	}
	if (notify) notify(notify_ctx, lead);
}

static void list_invoke(FluxListViewData *ld, int index) {
	if (!(ld->item_invoked_enabled && ld->sel_mode == XTK_LIST_SELECT_NONE)) return;
	void     (*notify)(void *, int) = ld->on_invoke;
	void    *notify_ctx             = ld->on_invoke_ctx;
	XtkHost *host                   = flux_store_host(ld->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_INVOKE,
		  .target         = ( XtkCtrl ) ld->root,
		  .source         = ( XtkCtrl ) ld->root,
		  .current_target = ( XtkCtrl ) ld->root,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = XTK_EVENT_CAUSE_USER,
		  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index},
		};
		flux_host_route(host, &event);
	}
	if (notify) notify(notify_ctx, index);
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

static bool key_down(FluxKey key) { return flux_key_held(key); }

/* Primary gesture (click / Space / Enter) — Interaction.cpp OnSelectItemPrimary. */
static void list_gesture_primary(FluxListViewData *ld, int index) {
	bool shift = key_down(FLUX_KEY_SHIFT);
	switch (ld->sel_mode) {
	case XTK_LIST_SELECT_NONE     : list_report(ld, index); break; /* ItemClick-style */
	case XTK_LIST_SELECT_SINGLE   : sel_single(ld, index); break;
	case XTK_LIST_SELECT_MULTIPLE : shift ? sel_range(ld, index, false) : sel_toggle(ld, index); break;
	case XTK_LIST_SELECT_EXTENDED : shift ? sel_range(ld, index, true) : sel_single(ld, index); break;
	}
}

/* Secondary gesture (Ctrl+click / Ctrl+Space) — OnSelectItemSecondary. */
static void list_gesture_secondary(FluxListViewData *ld, int index) {
	bool shift = key_down(FLUX_KEY_SHIFT);
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
	FluxNodeData *snd  = flux_store_find(ld->store, ld->scroll);
	XentRect      rect = {0};
	if (!snd || !snd->component_data || !xent_layout_rect(ld->ctx, ld->scroll, &rect) || rect.h <= 0.0f) return false;
	*offset = (( FluxScrollData * ) snd->component_data)->scroll_y;
	*extent = rect.h;
	return true;
}

/* Minimal-edge alignment (ScrollIntoViewAlignment_Default): scroll just far
 * enough that the cell's row is fully inside the viewport. */
void flux_list_scrollto(FluxStore *store, XentNodeId list, int index) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld || index < 0 || index >= ld->count) return;
	float offset = 0.0f, extent = 0.0f;
	if (!list_viewport_metrics(ld, &offset, &extent)) return;

	int   cols  = ld->cols > 0 ? ld->cols : 1;
	float y     = ( float ) (index / cols) * ld->item_height;
	float new_y = offset;
	if (y < offset) new_y = y;
	else if (y + ld->item_height > offset + extent) new_y = y + ld->item_height - extent;
	if (new_y != offset) flux_prop_scroll_offset(store, ld->scroll, 0.0f, new_y);
}

static XentNodeId list_find_item_node(FluxListViewData const *ld, int index) {
	for (XentNodeId child = xent_node_first(ld->ctx, ld->host); child != XENT_NODE_INVALID;
	  child               = xent_node_next(ld->ctx, child))
	{
		FluxNodeData *nd = flux_store_find(ld->store, child);
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
	flux_list_scrollto(ld->store, ld->root, index);

	XentNodeId node = list_find_item_node(ld, index);
	if (node != XENT_NODE_INVALID) {
		if (ld->input) flux_input_setfocus(ld->input, node);
		ld->pending_focus = -1;
	}
	else ld->pending_focus = index;
}

/* Arrow/Home/End/Page target — GridView 2D nav: Left/Right ±1, Up/Down ±cols. */
static int list_nav_target(FluxListViewData const *ld, int from, FluxKey key) {
	int   cols   = ld->cols > 0 ? ld->cols : 1;
	float extent = 0.0f, offset = 0.0f;
	int   page_rows = 8;
	if (list_viewport_metrics(ld, &offset, &extent) && ld->item_height > 0.0f)
		page_rows = ( int ) (extent / ld->item_height);
	if (page_rows < 1) page_rows = 1;

	switch (key) {
	case FLUX_KEY_UP        : return from - cols;
	case FLUX_KEY_DOWN      : return from + cols;
	case FLUX_KEY_LEFT      : return cols > 1 ? from - 1 : from;
	case FLUX_KEY_RIGHT     : return cols > 1 ? from + 1 : from;
	case FLUX_KEY_HOME      : return 0;
	case FLUX_KEY_END       : return ld->count - 1;
	case FLUX_KEY_PAGE_UP   : return from - page_rows * cols;
	case FLUX_KEY_PAGE_DOWN : return from + page_rows * cols;
	default                 : return from;
	}
}

/* Selection-follows-focus rules — Interaction.cpp OnKeyboardNavigation. */
static void list_nav_select(FluxListViewData *ld, int index) {
	bool ctrl  = key_down(FLUX_KEY_CONTROL);
	bool shift = key_down(FLUX_KEY_SHIFT);

	if (ld->sel_mode == XTK_LIST_SELECT_EXTENDED && shift) {
		sel_range(ld, index, !ctrl);
		return;
	}
	if (ld->sel_mode == XTK_LIST_SELECT_MULTIPLE) {
		ld->anchor     = index; /* Multiple: anchor always follows focus */
		ld->anchor_set = true;
		if (shift) sel_range(ld, index, false);
		return;
	}
	if (ctrl) return; /* focus-only move */
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE && ld->single_follows_focus) sel_single(ld, index);
	if (ld->sel_mode == XTK_LIST_SELECT_EXTENDED) sel_single(ld, index);
}

/* Arrow/Home/End/Page: clamp the target, move selection per mode, focus it.
 * A no-op move still swallows the key (except Home/End, which always land). */
static bool list_key_navigate(FluxListViewData *ld, FluxListItemData const *it, FluxKey key) {
	int target = list_nav_target(ld, it->index, key);
	if (target < 0) target = 0;
	if (target > ld->count - 1) target = ld->count - 1;
	if (target == it->index && key != FLUX_KEY_HOME && key != FLUX_KEY_END) return true;
	FluxStore *store = ld->store;
	XentNodeId root  = ld->root;
	list_nav_select(ld, target);
	ld = list_data(store, root);
	if (ld) list_focus_index(ld, target);
	return true;
}

static void list_invoke_if_live(FluxStore *store, XentNodeId root, int index) {
	FluxListViewData *ld = list_data(store, root);
	if (ld) list_invoke(ld, index);
}

static bool list_item_activate(FluxListViewData *ld, FluxListItemData const *it, bool secondary) {
	FluxStore *store = ld->store;
	XentNodeId root  = ld->root;
	int        index = it->index;
	if (secondary) list_gesture_secondary(ld, index);
	else list_gesture_primary(ld, index);
	list_invoke_if_live(store, root, index);
	return true;
}

static bool list_item_select_all(FluxListViewData *ld) {
	if (!key_down(FLUX_KEY_CONTROL) || key_down(FLUX_KEY_SHIFT) || !multi_mode(ld)) return false;
	sel_all_toggle(ld);
	return true;
}

static bool list_item_on_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxListItemData *it = ( FluxListItemData * ) ctx;
	FluxListViewData *ld = it->owner;
	if (!ld || ld->kind == XTK_LIST_KIND_REPEATER || ld->count <= 0) return false;

	switch (key) {
	case FLUX_KEY_UP :
	case FLUX_KEY_DOWN :
	case FLUX_KEY_LEFT :
	case FLUX_KEY_RIGHT :
	case FLUX_KEY_HOME :
	case FLUX_KEY_END :
	case FLUX_KEY_PAGE_UP :
	case FLUX_KEY_PAGE_DOWN : return list_key_navigate(ld, it, key);
	case FLUX_KEY_SPACE     : return list_item_activate(ld, it, key_down(FLUX_KEY_CONTROL));
	case FLUX_KEY_ENTER     : return list_item_activate(ld, it, false);
	case 'A'                : return list_item_select_all(ld);
	default                 : return false;
	}
}

/* Click (fires on release) routes through the same gesture machinery so
 * Ctrl/Shift+click follow WinUI exactly. */
static void list_item_on_click(void *ctx) {
	FluxListItemData *it = ( FluxListItemData * ) ctx;
	FluxListViewData *ld = it->owner;
	if (!ld || ld->kind == XTK_LIST_KIND_REPEATER) return;
	FluxStore *store = ld->store;
	XentNodeId root  = ld->root;
	int        index = it->index;
	ld->focused      = index;
	if (key_down(FLUX_KEY_CONTROL)) list_gesture_secondary(ld, index);
	else list_gesture_primary(ld, index);
	list_invoke_if_live(store, root, index);
}

/* -------------------------------------------------------------------------
 * Create / destroy
 * ---------------------------------------------------------------------- */

static void list_view_data_destroy(void *component_data) {
	FluxListViewData *ld = ( FluxListViewData * ) component_data;
	if (!ld) return;
	FluxNodeData   *snd = flux_store_find(ld->store, ld->scroll);
	FluxScrollData *sd  = snd ? ( FluxScrollData * ) snd->component_data : NULL;
	if (sd && sd->on_offset_changed_ctx == ld) {
		sd->on_offset_changed     = NULL;
		sd->on_offset_changed_ctx = NULL;
	}
	free(ld->ranges);
	free(ld);
}

XentNodeId flux_list_init(FluxListViewDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	FluxControlType type = list_type_for_kind(info->kind);
	XentNodeId      root = flux_factory_create_node(info->ctx, info->store, info->parent, type);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_store_find(info->store, root);
	FluxListViewData *ld = nd ? ( FluxListViewData * ) calloc(1, sizeof(FluxListViewData)) : NULL;
	if (!nd || !ld) {
		free(ld);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	xent_setproto(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_setflexdir(info->ctx, root, XENT_FLEX_COLUMN);
	xent_setitems(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);

	XentNodeId scroll = flux_scroll_init(&(FluxContainerDesc) {info->ctx, info->store, root});
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
	xent_setgrow(info->ctx, scroll, 1.0f);
	xent_setbasis(info->ctx, scroll, 0.0f);
	xent_setproto(info->ctx, host, XENT_PROTOCOL_ABSOLUTE);
	xent_setwpct(info->ctx, host, 1.0f);
	xent_seth(info->ctx, host, 0.0f);
	xent_setshrink(info->ctx, host, 0.0f);

	ld->ctx                    = info->ctx;
	ld->store                  = info->store;
	ld->input                  = info->input;
	ld->root                   = root;
	ld->scroll                 = scroll;
	ld->host                   = host;
	ld->kind                   = info->kind;
	ld->cols                   = 1;
	ld->selected               = -1;
	ld->realized_last          = -1;
	ld->focused                = -1;
	ld->pending_focus          = -1;
	ld->single_follows_focus   = true;
	ld->on_select              = info->on_select;
	ld->on_select_ctx          = info->userdata;

	nd->component_data         = ld;
	nd->destroy_component_data = list_view_data_destroy;

	xent_sem_setrole(info->ctx, root, XENT_SEM_ROLE_CONTAINER);
	return root;
}

static FluxListViewData *list_item_owner(XentCtx *ctx, FluxStore *store, XentNodeId parent) {
	XentNodeId scroll = xent_node_parent(ctx, parent);
	XentNodeId root   = scroll != XENT_NODE_INVALID ? xent_node_parent(ctx, scroll) : XENT_NODE_INVALID;
	return root != XENT_NODE_INVALID ? list_data(store, root) : NULL;
}

static void list_item_wire_interactive(XentCtx *ctx, XentNodeId node, FluxNodeData *nd, FluxListItemData *it) {
	if (it->owner && it->owner->kind == XTK_LIST_KIND_REPEATER) return;
	nd->behavior.on_click     = list_item_on_click;
	nd->behavior.on_click_ctx = it;
	nd->behavior.on_key       = list_item_on_key;
	nd->behavior.on_key_ctx   = it;
	xent_set_focusable(ctx, node, true);
	xent_sem_setrole(ctx, node, XENT_SEM_ROLE_BUTTON);
}

static void list_item_apply_chrome(XentCtx *ctx, XentNodeId node, FluxListViewData const *owner) {
	xent_setproto(ctx, node, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, node, XENT_FLEX_COLUMN);
	xent_setjustify(ctx, node, XENT_FLEX_JUSTIFY_CENTER);
	if (owner && owner->kind == XTK_LIST_KIND_GRID) {
		xent_setitems(ctx, node, XENT_FLEX_ALIGN_CENTER);
		xent_setp(ctx, node, (XentInsets) {0.0f, 0.0f, 4.0f, 4.0f});
	}
	else if (owner && owner->kind == XTK_LIST_KIND_LIST_BOX) {
		xent_setitems(ctx, node, XENT_FLEX_ALIGN_STRETCH);
		xent_setp(ctx, node, (XentInsets) {12.0f, 9.0f, 12.0f, 12.0f});
	}
	else {
		xent_setitems(ctx, node, XENT_FLEX_ALIGN_STRETCH);
		xent_setp(ctx, node, (XentInsets) {16.0f, 0.0f, 12.0f, 0.0f});
	}
	if (!owner || owner->item_width <= 0.0f) xent_setwpct(ctx, node, 1.0f);
}

XentNodeId flux_listitem_init(FluxListItemDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_LIST_ITEM);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_store_find(info->store, node);
	FluxListItemData *it = nd ? ( FluxListItemData * ) calloc(1, sizeof(FluxListItemData)) : NULL;
	if (!nd || !it) {
		free(it);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	it->owner                  = list_item_owner(info->ctx, info->store, info->parent);
	it->index                  = -1;
	nd->component_data         = it;
	nd->destroy_component_data = free;
	list_item_wire_interactive(info->ctx, node, nd, it);
	list_item_apply_chrome(info->ctx, node, it->owner);
	return node;
}

/* -------------------------------------------------------------------------
 * Store-facing setters / accessors
 * ---------------------------------------------------------------------- */

XentNodeId flux_list_content_node(FluxStore *store, XentNodeId list) {
	FluxListViewData *ld = list_data(store, list);
	return ld ? ld->host : XENT_NODE_INVALID;
}

static FluxScrollData *list_scroll_data(FluxListViewData const *ld) {
	FluxNodeData *snd = flux_store_find(ld->store, ld->scroll);
	return snd ? ( FluxScrollData * ) snd->component_data : NULL;
}

/* Virtual extent: the rows × item_height content exists only as the scroll's
 * manual logical extent (scrollbar + clamping). The host stays a small
 * physical canvas spanning just the realized window (set_realized), so the
 * compositor / hit paths never see deep-scroll coordinates. */
static void list_apply_host_extent(FluxListViewData *ld) {
	int             cols = ld->cols > 0 ? ld->cols : 1;
	int             rows = cols > 0 ? (ld->count + cols - 1) / cols : ld->count;
	FluxScrollData *sd   = list_scroll_data(ld);
	if (!sd) return;
	sd->content_manual = 1;
	sd->virtualized    = 1;
	sd->content_w      = 0.0f;
	sd->content_h      = ( float ) rows * ld->item_height;
}

void flux_list_set_extent(
  FluxStore *store, XentNodeId list, int count, float item_height, float item_width, int columns
) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return;
	if (count < 0) count = 0;
	if (item_height <= 0.0f) item_height = 40.0f;
	if (ld->count == count && ld->item_height == item_height && ld->item_width == item_width && ld->columns == columns)
		return;
	ld->count       = count;
	ld->item_height = item_height;
	ld->item_width  = item_width;
	ld->columns     = columns;
	list_apply_host_extent(ld);
}

void flux_list_setmode(FluxStore *store, XentNodeId list, XtkListSelMode mode) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld || ld->sel_mode == mode) return;
	ld->sel_mode   = mode;
	ld->anchor_set = false;
	ranges_clear(ld);
}

void flux_list_set_selected(FluxStore *store, XentNodeId list, int selected) {
	FluxListViewData *ld = list_data(store, list);
	if (ld) ld->selected = selected;
}

bool flux_list_is_selected(FluxStore *store, XentNodeId list, int index) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return false;
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) return index == ld->selected;
	return ranges_contains(ld, index);
}

int flux_list_selection_count(FluxStore *store, XentNodeId list) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return 0;
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) return ld->selected >= 0 ? 1 : 0;
	return ranges_total(ld);
}

static float list_cell_width(FluxListViewData const *ld) {
	if (ld->item_width > 0.0f) return ld->item_width;
	return 0.0f;
}

static void list_place_children(FluxListViewData *ld) {
	int   c      = ld->cols > 0 ? ld->cols : 1;
	float cell_w = list_cell_width(ld);
	int   index  = ld->realized_first;
	bool  multi  = ld->sel_mode == XTK_LIST_SELECT_MULTIPLE;
	for (XentNodeId child = xent_node_first(ld->ctx, ld->host); child != XENT_NODE_INVALID;
	  child               = xent_node_next(ld->ctx, child))
	{
		if (ld->realized_last >= 0 && index > ld->realized_last) break;
		float x = c > 1 ? ( float ) (index % c) * cell_w : 0.0f;
		float y = ( float ) (index / c) * ld->item_height;
		fluxlistitem_set_place(ld->store, child, index, x, y);
		bool selected = ld->sel_mode == XTK_LIST_SELECT_SINGLE ? index == ld->selected
		                                                       : flux_list_is_selected(ld->store, ld->root, index);
		fluxlistitem_set_state(ld->store, child, selected, multi);
		index++;
	}
}

void flux_list_set_realized(FluxStore *store, XentNodeId list, int first, int last, int cols) {
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
	xent_seth(ld->ctx, ld->host, span);

	list_place_children(ld);

	FluxScrollData *sd = list_scroll_data(ld);
	if (!sd || sd->origin_y == origin) return;
	sd->origin_y = origin;
	for (XentNodeId child = xent_node_first(ld->ctx, ld->host); child != XENT_NODE_INVALID;
	  child               = xent_node_next(ld->ctx, child))
	{
		FluxListItemData *it = item_data(store, child);
		if (it && it->index >= 0)
			xent_setpos(ld->ctx, child, (XentPoint) {it->logical_x - sd->origin_x, it->logical_y - origin});
	}
}

void flux_list_stage_realized(FluxStore *store, XentNodeId list, int first, int last, int cols) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return;
	ld->pending_first   = first;
	ld->pending_last    = last;
	ld->pending_cols    = cols > 0 ? cols : 1;
	ld->realize_pending = true;
}

void flux_list_apply_pending(FluxStore *store, XentNodeId list) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld || ld->host == XENT_NODE_INVALID) return;
	if (ld->realize_pending) {
		flux_list_set_realized(store, list, ld->pending_first, ld->pending_last, ld->pending_cols);
		ld->realize_pending = false;
		return;
	}
	if (ld->realized_last < 0) {
		int n = xent_node_nchild(ld->ctx, ld->host);
		if (n <= 0) return;
		if (ld->item_height <= 0.0f) ld->item_height = 40.0f;
		flux_list_set_realized(store, list, 0, n - 1, ld->cols > 0 ? ld->cols : 1);
		return;
	}
	list_place_children(ld);
}

static void list_scroll_changed(void *ctx) {
	FluxListViewData *ld = ( FluxListViewData * ) ctx;
	if (!ld) return;
	FluxNodeData *nd = flux_store_find(ld->store, ld->root);
	if (nd && flux_list_window_stale(ld->ctx, nd)) flux_list_notify_stale(ld->store, ld->root);
}

void flux_list_onstale(FluxStore *store, XentNodeId list, void (*on_stale)(void *), void *ctx) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return;
	ld->on_stale       = on_stale;
	ld->on_stale_ctx   = ctx;
	FluxScrollData *sd = list_scroll_data(ld);
	if (!sd) return;
	sd->on_offset_changed     = list_scroll_changed;
	sd->on_offset_changed_ctx = ld;
}

bool flux_list_viewport(FluxStore *store, XentNodeId list, float *offset, float *extent, float *cross) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return false;
	XentRect rect = {0};
	if (!list_viewport_metrics(ld, offset, extent)) return false;
	*cross = xent_layout_rect(ld->ctx, ld->scroll, &rect) ? rect.w : 0.0f;
	return true;
}

bool flux_list_window_stale(XentCtx *ctx, struct FluxNodeData *nd) {
	FluxListViewData *ld = ( FluxListViewData * ) nd->component_data;
	if (!ld || !ld->on_stale || ld->stale_fired || ld->count <= 0 || ld->item_height <= 0.0f) return false;

	FluxNodeData *snd  = flux_store_find(ld->store, ld->scroll);
	XentRect      rect = {0};
	if (!snd || !snd->component_data || !xent_layout_rect(ctx, ld->scroll, &rect) || rect.h <= 0.0f) return false;
	float offset     = (( FluxScrollData * ) snd->component_data)->scroll_y;

	int   cols       = xtk_list_auto_columns(rect.w, ld->item_width, ld->columns);
	int   total_rows = (ld->count + cols - 1) / cols;
	int   row_first  = ( int ) floorf(offset / ld->item_height);
	int   row_last   = ( int ) ceilf((offset + rect.h) / ld->item_height) - 1;
	if (row_first < 0) row_first = 0;
	if (row_last > total_rows - 1) row_last = total_rows - 1;

	int visible_first = row_first * cols;
	int visible_last  = (row_last + 1) * cols - 1;
	if (visible_last > ld->count - 1) visible_last = ld->count - 1;

	return cols != ld->cols || visible_first < ld->realized_first || visible_last > ld->realized_last;
}

void flux_list_notify_stale(FluxStore *store, XentNodeId list) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld || !ld->on_stale || ld->stale_fired) return;
	ld->stale_fired = true;
	ld->on_stale(ld->on_stale_ctx);
}

void fluxlistitem_bind_owner(FluxStore *store, XentNodeId item) {
	FluxListItemData *it = item_data(store, item);
	if (!it || it->owner) return;
	XentCtx   *ctx    = flux_store_ctx(store);
	XentNodeId parent = xent_node_parent(ctx, item);
	it->owner         = parent != XENT_NODE_INVALID ? list_item_owner(ctx, store, parent) : NULL;
}

static void list_item_set_slot_size(FluxListItemData const *it, XentNodeId item) {
	FluxListViewData *owner = it->owner;
	if (!owner) return;
	xent_seth(owner->ctx, item, owner->item_height);
	if (owner->item_width > 0.0f) xent_setw(owner->ctx, item, owner->item_width);
}

static void list_item_complete_focus(FluxListItemData *it, XentNodeId item) {
	FluxListViewData *owner = it->owner;
	if (!owner || owner->pending_focus != it->index) return;
	owner->pending_focus = -1;
	if (owner->input) flux_input_setfocus(owner->input, item);
}

void fluxlistitem_set_place(FluxStore *store, XentNodeId item, int index, float x, float y) {
	FluxListItemData *it = item_data(store, item);
	if (!it) return;
	fluxlistitem_bind_owner(store, item);
	list_item_set_slot_size(it, item);
	it->index          = index;
	it->logical_x      = x;
	it->logical_y      = y;
	FluxScrollData *sd = it->owner ? list_scroll_data(it->owner) : NULL;
	xent_setpos(
	  flux_store_ctx(store), item, (XentPoint) {x - (sd ? sd->origin_x : 0.0f), y - (sd ? sd->origin_y : 0.0f)}
	);
	list_item_complete_focus(it, item);
}

void fluxlistitem_set_state(FluxStore *store, XentNodeId item, bool selected, bool multi) {
	FluxListItemData *it = item_data(store, item);
	if (!it) return;
	it->selected = selected;
	if (it->multi != multi) {
		it->multi = multi;
		/* Multiple mode reserves the 20px checkbox + margins on the leading
		 * edge (rounded content offset 28). Lists only — grid overlays. */
		if (it->owner && it->owner->kind == XTK_LIST_KIND_LIST) {
			XentInsets pad
			  = multi ? (XentInsets) {16.0f + 28.0f, 0.0f, 12.0f, 0.0f} : (XentInsets) {16.0f, 0.0f, 12.0f, 0.0f};
			xent_setp(flux_store_ctx(store), item, pad);
		}
	}
}
