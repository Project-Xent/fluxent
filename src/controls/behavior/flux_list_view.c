#include "controls/flux_control_uia.h"
#include "controls/behavior/flux_list_internal.h"
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
	if (!nd || !nd->component_data) return NULL;
	if (nd->component_type != FLUX_CONTROL_LIST_ITEM && nd->component_type != FLUX_CONTROL_GROUP_HEADER) return NULL;
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

static bool multi_mode(FluxListViewData const *ld) {
	return ld->sel_mode == XTK_LIST_SELECT_MULTIPLE || ld->sel_mode == XTK_LIST_SELECT_EXTENDED;
}

void flux_list_report(FluxListViewData *ld, int lead) {
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

static void list_select_one(FluxListViewData *ld, int index) {
	ld->selected = index;
	for (XentNodeId c = flux_list_next_row(ld, XENT_NODE_INVALID); c != XENT_NODE_INVALID;
	  c               = flux_list_next_row(ld, c))
	{
		FluxListItemData const *it = item_data(ld->store, c);
		if (it) fluxlistitem_set_state(ld->store, c, it->index >= 0 && it->index == index, false);
	}
	flux_list_report(ld, index);
}

static void sel_single(FluxListViewData *ld, int index) {
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) {
		list_select_one(ld, index);
		return;
	}
	flux_ranges_clear(ld);
	flux_ranges_add(ld, index, index);
	ld->anchor     = index;
	ld->anchor_set = true;
	flux_list_report(ld, index);
}

static void sel_toggle(FluxListViewData *ld, int index) {
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) {
		list_select_one(ld, ld->selected == index ? -1 : index);
		return;
	}
	if (flux_ranges_contains(ld, index)) flux_ranges_remove(ld, index);
	else flux_ranges_add(ld, index, index);
	ld->anchor     = index;
	ld->anchor_set = true;
	flux_list_report(ld, index);
}

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
	int  lo     = anchor < index ? anchor : index;
	int  hi     = anchor < index ? index : anchor;
	bool remove = ld->sel_mode == XTK_LIST_SELECT_MULTIPLE && !flux_ranges_contains(ld, anchor);
	if (clear_old) flux_ranges_clear(ld);
	if (remove) flux_ranges_cut(ld, lo, hi);
	else flux_ranges_add(ld, lo, hi);
	flux_list_report(ld, index);
}

static void sel_all_toggle(FluxListViewData *ld) {
	if (!multi_mode(ld) || ld->count <= 0) return;
	bool all = ld->range_count == 1 && ld->ranges [0].first == 0 && ld->ranges [0].last == ld->count - 1;
	flux_ranges_clear(ld);
	if (!all) flux_ranges_add(ld, 0, ld->count - 1);
	flux_list_report(ld, all ? -1 : ld->count - 1);
}

static bool key_down(FluxKey key) { return flux_key_held(key); }

static void list_gesture_primary(FluxListViewData *ld, int index) {
	bool shift = key_down(FLUX_KEY_SHIFT);
	switch (ld->sel_mode) {
	case XTK_LIST_SELECT_NONE     : flux_list_report(ld, index); break;
	case XTK_LIST_SELECT_SINGLE   : sel_single(ld, index); break;
	case XTK_LIST_SELECT_MULTIPLE : shift ? sel_range(ld, index, false) : sel_toggle(ld, index); break;
	case XTK_LIST_SELECT_EXTENDED : shift ? sel_range(ld, index, true) : sel_single(ld, index); break;
	}
}

static void list_gesture_secondary(FluxListViewData *ld, int index) {
	bool shift = key_down(FLUX_KEY_SHIFT);
	switch (ld->sel_mode) {
	case XTK_LIST_SELECT_NONE     : flux_list_report(ld, index); break;
	case XTK_LIST_SELECT_SINGLE   : sel_toggle(ld, index); break;
	case XTK_LIST_SELECT_MULTIPLE : shift ? sel_range(ld, index, false) : sel_toggle(ld, index); break;
	case XTK_LIST_SELECT_EXTENDED : shift ? sel_range(ld, index, false) : sel_toggle(ld, index); break;
	}
}

static bool list_viewport_metrics(FluxListViewData const *ld, float *offset, float *extent) {
	FluxNodeData *snd  = flux_store_find(ld->store, ld->scroll);
	XentRect      rect = {0};
	if (!snd || !snd->component_data || !xent_layout_rect(ld->ctx, ld->scroll, &rect) || rect.h <= 0.0f) return false;
	*offset = (( FluxScrollData * ) snd->component_data)->scroll_y;
	*extent = rect.h;
	return true;
}

static void list_scroll_row(FluxListViewData *ld, int row) {
	float offset = 0.0f, extent = 0.0f;
	if (!list_viewport_metrics(ld, &offset, &extent)) return;
	int   line  = flux_list_line_of(ld, row);
	float y     = flux_list_row_y(ld, line);
	float h     = flux_list_row_h(ld, line);
	float new_y = offset;
	if (y < offset) new_y = y;
	else if (y + h > offset + extent) new_y = y + h - extent;
	if (new_y != offset) flux_prop_scroll_offset(ld->store, ld->scroll, 0.0f, new_y);
}

void flux_list_scrollto(FluxStore *store, XentNodeId list, int index) {
	FluxListViewData *ld = list_data(store, list);
	if (ld && index >= 0 && index < ld->count) list_scroll_row(ld, flux_list_row_of_item(ld, index));
}

static XentNodeId list_find_row_node(FluxListViewData const *ld, int row) {
	for (XentNodeId c = flux_list_next_row(ld, XENT_NODE_INVALID); c != XENT_NODE_INVALID;
	  c               = flux_list_next_row(ld, c))
	{
		FluxListItemData const *it = item_data(ld->store, c);
		if (it && it->row == row) return c;
	}
	return XENT_NODE_INVALID;
}

static void list_focus_row(FluxListViewData *ld, int row) {
	list_scroll_row(ld, row);
	XentNodeId node = list_find_row_node(ld, row);
	if (node != XENT_NODE_INVALID) {
		if (ld->input) flux_input_setfocus(ld->input, node, false);
		ld->pending_focus = -1;
	}
	else ld->pending_focus = row;
}

static int list_page_rows(FluxListViewData const *ld) {
	float extent = 0.0f, offset = 0.0f;
	int   rows = list_viewport_metrics(ld, &offset, &extent) ? ( int ) (extent / flux_list_pitch(ld)) : 8;
	return rows > 1 ? rows : 1;
}

static int list_nav_line(FluxListViewData const *ld, int from, int delta) {
	int line  = flux_list_line_of(ld, from) + delta;
	int lines = flux_list_lines(ld);
	if (line > lines - 1) line = lines - 1;
	if (line < 0) line = 0;
	int first = flux_list_line_first(ld, line), last = flux_list_line_last(ld, line);
	int at = first + flux_list_col_of(ld, from);
	return at < last ? at : last;
}

static int list_nav_target(FluxListViewData const *ld, int from, FluxKey key) {
	bool grid = ld->cols > 1;
	switch (key) {
	case FLUX_KEY_UP        : return list_nav_line(ld, from, -1);
	case FLUX_KEY_DOWN      : return list_nav_line(ld, from, 1);
	case FLUX_KEY_LEFT      : return grid ? from - 1 : from;
	case FLUX_KEY_RIGHT     : return grid ? from + 1 : from;
	case FLUX_KEY_HOME      : return flux_list_row_of_item(ld, 0);
	case FLUX_KEY_END       : return flux_list_row_of_item(ld, ld->count - 1);
	case FLUX_KEY_PAGE_UP   : return list_nav_line(ld, from, -list_page_rows(ld));
	case FLUX_KEY_PAGE_DOWN : return list_nav_line(ld, from, list_page_rows(ld));
	default                 : return from;
	}
}

static void list_nav_select(FluxListViewData *ld, int index) {
	bool ctrl  = key_down(FLUX_KEY_CONTROL);
	bool shift = key_down(FLUX_KEY_SHIFT);

	if (ld->sel_mode == XTK_LIST_SELECT_EXTENDED && shift) {
		sel_range(ld, index, !ctrl);
		return;
	}
	if (ld->sel_mode == XTK_LIST_SELECT_MULTIPLE) {
		ld->anchor     = index;
		ld->anchor_set = true;
		if (shift) sel_range(ld, index, false);
		return;
	}
	if (ctrl) return;
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE && ld->single_follows_focus) sel_single(ld, index);
	if (ld->sel_mode == XTK_LIST_SELECT_EXTENDED) sel_single(ld, index);
}

static bool list_key_navigate(FluxListViewData *ld, FluxListItemData const *it, FluxKey key) {
	int last   = ld->count + ld->ngroup - 1;
	int target = list_nav_target(ld, it->row, key);
	if (target > last) target = last;
	if (target < 0) target = 0;
	if (target == it->row && key != FLUX_KEY_HOME && key != FLUX_KEY_END) return true;
	FluxStore *store = ld->store;
	XentNodeId root  = ld->root;
	int        item  = flux_list_item_of_row(ld, target);
	if (item >= 0) list_nav_select(ld, item);
	ld = list_data(store, root);
	if (ld) list_focus_row(ld, target);
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
	if (it->header && (key == FLUX_KEY_SPACE || key == FLUX_KEY_ENTER)) return false;

	switch (key) {
	case FLUX_KEY_UP        :
	case FLUX_KEY_DOWN      :
	case FLUX_KEY_LEFT      :
	case FLUX_KEY_RIGHT     :
	case FLUX_KEY_HOME      :
	case FLUX_KEY_END       :
	case FLUX_KEY_PAGE_UP   :
	case FLUX_KEY_PAGE_DOWN : return list_key_navigate(ld, it, key);
	case FLUX_KEY_SPACE     : return list_item_activate(ld, it, key_down(FLUX_KEY_CONTROL));
	case FLUX_KEY_ENTER     : return list_item_activate(ld, it, false);
	case FLUX_KEY_A         : return list_item_select_all(ld);
	default                 : return false;
	}
}

static void list_item_on_click(void *ctx) {
	FluxListItemData *it = ( FluxListItemData * ) ctx;
	FluxListViewData *ld = it->owner;
	if (!ld || ld->kind == XTK_LIST_KIND_REPEATER || it->header) return;
	FluxStore *store = ld->store;
	XentNodeId root  = ld->root;
	int        index = it->index;
	ld->focused      = index;
	if (key_down(FLUX_KEY_CONTROL)) list_gesture_secondary(ld, index);
	else list_gesture_primary(ld, index);
	list_invoke_if_live(store, root, index);
}

static void list_view_data_destroy(void *component_data) {
	FluxListViewData *ld = ( FluxListViewData * ) component_data;
	if (!ld) return;
	FluxNodeData   *snd = flux_store_find(ld->store, ld->scroll);
	FluxScrollData *sd  = snd ? ( FluxScrollData * ) snd->component_data : NULL;
	if (sd && sd->on_offset_changed_ctx == ld) {
		sd->on_offset_changed     = NULL;
		sd->on_offset_changed_ctx = NULL;
	}
	flux_list_release(ld);
	free(ld);
}

void flux_list_release(FluxListViewData *ld) {
	free(ld->ranges);
	free(ld->item_sizes.slots);
	free(ld->header_sizes.slots);
	free(ld->groups);
	free(ld->band_prev);
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
	ld->header                 = XENT_NODE_INVALID;
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
	if (info->realize_events) flux_list_emitrealize(ld);

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

XentNodeId flux_groupheader_init(FluxListItemDesc const *info) {
	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_GROUP_HEADER);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;
	FluxNodeData     *nd = flux_store_find(info->store, node);
	FluxListItemData *it = nd ? ( FluxListItemData * ) calloc(1, sizeof(*it)) : NULL;
	if (!it) return flux_factory_fail_node(info->ctx, info->store, node);
	*it                = (FluxListItemData) {.index = -1, .row = -1, .group = -1, .header = true, .extent = -1.0f};
	nd->component_data = it;
	nd->destroy_component_data = free;
	nd->behavior.on_key        = list_item_on_key;
	nd->behavior.on_key_ctx    = it;
	xent_setproto(info->ctx, node, XENT_PROTOCOL_FLEX);
	xent_setflexdir(info->ctx, node, XENT_FLEX_COLUMN);
	xent_setitems(info->ctx, node, XENT_FLEX_ALIGN_START);
	xent_setp(info->ctx, node, (XentInsets) {12.0f, 8.0f, 12.0f, 13.0f});
	xent_setwpct(info->ctx, node, 1.0f);
	xent_setminsize(info->ctx, node, (XentSize) {0.0f, 48.0f});
	xent_setfontsize(info->ctx, node, 20.0f);
	xent_set_focusable(info->ctx, node, true);
	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_CONTAINER);
	return node;
}

XentNodeId flux_list_content_node(FluxStore *store, XentNodeId list) {
	FluxListViewData *ld = list_data(store, list);
	return ld ? ld->host : XENT_NODE_INVALID;
}

FluxScrollData *flux_list_scroll(FluxListViewData const *ld) {
	FluxNodeData *snd = flux_store_find(ld->store, ld->scroll);
	return snd ? ( FluxScrollData * ) snd->component_data : NULL;
}

static int list_autocolumns(float cross, float itemw, int explicit_columns) {
	if (explicit_columns > 0) return explicit_columns;
	if (itemw <= 0.0f || cross <= 0.0f) return 1;
	int cols = ( int ) floorf(cross / itemw);
	return cols > 0 ? cols : 1;
}

static int list_columns(FluxListViewData const *ld) {
	XentRect rect = {0};
	float    w    = xent_layout_rect(ld->ctx, ld->scroll, &rect) ? rect.w : 0.0f;
	return list_autocolumns(w, ld->item_width, ld->columns);
}

static void list_apply_host_extent(FluxListViewData *ld) {
	FluxScrollData *sd = flux_list_scroll(ld);
	if (!sd) return;
	sd->content_manual = 1;
	sd->virtualized    = 1;
	sd->content_w      = ld->header_w;
	sd->content_h      = flux_list_extent(ld);
}

static void list_place_header(FluxListViewData const *ld, FluxScrollData const *sd) {
	if (ld->header == XENT_NODE_INVALID) return;
	xent_setpos(ld->ctx, ld->header, (XentPoint) {-(sd ? sd->origin_x : 0.0f), -(sd ? sd->origin_y : 0.0f)});
}

void flux_list_set_extent(
  FluxStore *store, XentNodeId list, int count, float item_height, float item_width, int columns
) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return;
	if (count < 0) count = 0;
	if (item_height < 0.0f) item_height = 0.0f;
	if (item_height == 0.0f && (item_width > 0.0f || columns > 1)) item_height = 40.0f;
	if (ld->count == count && ld->item_height == item_height && ld->item_width == item_width && ld->columns == columns)
		return;
	ld->count       = count;
	ld->item_height = item_height;
	ld->item_width  = item_width;
	ld->columns     = columns;
	list_apply_host_extent(ld);
}

bool flux_list_groups(FluxStore *store, XentNodeId list, void const *sizes, int count) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld || !flux_list_setgroups(ld, sizes, count)) return false;
	ld->pinned = false;
	list_apply_host_extent(ld);
	return true;
}

void flux_list_setmode(FluxStore *store, XentNodeId list, XtkListSelMode mode) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld || ld->sel_mode == mode) return;
	ld->sel_mode   = mode;
	ld->anchor_set = false;
	flux_ranges_clear(ld);
}

void flux_list_set_selected(FluxStore *store, XentNodeId list, int selected) {
	FluxListViewData *ld = list_data(store, list);
	if (ld) ld->selected = selected;
}

bool flux_list_is_selected(FluxStore *store, XentNodeId list, int index) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return false;
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) return index == ld->selected;
	return flux_ranges_contains(ld, index);
}

FluxListItemData *flux_list_item(FluxListViewData const *ld, XentNodeId node) { return item_data(ld->store, node); }

XentNodeId        flux_list_next_row(FluxListViewData const *ld, XentNodeId child) {
	child = child == XENT_NODE_INVALID ? xent_node_first(ld->ctx, ld->host) : xent_node_next(ld->ctx, child);
	if (child != XENT_NODE_INVALID && child == ld->header) child = xent_node_next(ld->ctx, child);
	return child;
}

static bool list_row_selected(FluxListViewData const *ld, int index) {
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) return index == ld->selected;
	return flux_list_is_selected(ld->store, ld->root, index);
}

static XentNodeId list_place_detached(FluxListViewData *ld) {
	XentNodeId child    = flux_list_next_row(ld, XENT_NODE_INVALID);
	int        detached = flux_list_detached(ld);
	if (detached < 0 || child == XENT_NODE_INVALID) return child;
	fluxlistitem_set_place(ld->store, child, detached, -1, 0.0f, flux_list_row_y(ld, flux_list_line_of(ld, detached)));
	return flux_list_next_row(ld, child);
}

static float list_row_y(FluxListViewData const *ld, FluxFlowCursor *at, XentNodeId child, int row) {
	int   line = flux_list_line_of(ld, row);
	float size = flux_list_size_for(ld, item_data(ld->store, child), row);
	flux_flow_advance(at, line, size);
	return flux_list_measured(ld) ? at->y : ld->header_h + ( float ) line * ld->item_height;
}

static float list_place_children(FluxListViewData *ld) {
	bool           multi = ld->sel_mode == XTK_LIST_SELECT_MULTIPLE;
	FluxFlowCursor at    = {-1, ld->block_top, 0.0f};
	int            row   = ld->realized_first;
	for (XentNodeId child = list_place_detached(ld); child != XENT_NODE_INVALID && row <= ld->realized_last;
	  child               = flux_list_next_row(ld, child), row++)
	{
		int   index = flux_list_item_of_row(ld, row);
		float x     = ld->cols > 1 && index >= 0 ? ( float ) flux_list_col_of(ld, row) * ld->item_width : 0.0f;
		float y     = list_row_y(ld, &at, child, row);
		fluxlistitem_set_place(ld->store, child, row, index, x, y);
		fluxlistitem_set_state(ld->store, child, index >= 0 && list_row_selected(ld, index), multi);
	}
	return at.y + at.h;
}

static void list_uniform_block(FluxListViewData *ld) {
	int first     = flux_list_line_of(ld, ld->realized_first);
	int lines     = ld->realized_last >= ld->realized_first ? flux_list_line_of(ld, ld->realized_last) - first + 1 : 0;
	ld->block_top = ld->header_h + ( float ) first * ld->item_height;
	ld->block_bottom = ld->block_top + ( float ) lines * ld->item_height;
}

void flux_list_layout_block(FluxListViewData *ld) {
	FluxScrollData *sd     = flux_list_scroll(ld);
	bool            filled = ld->realized_last >= ld->realized_first;
	float           origin = flux_list_line_of(ld, ld->realized_first) > 0 ? ld->block_top : 0.0f;
	if (sd) sd->origin_y = origin;
	list_place_header(ld, sd);
	float end = list_place_children(ld);
	if (flux_list_measured(ld)) ld->block_bottom = filled ? end : ld->block_top;
	xent_seth(ld->ctx, ld->host, (filled ? ld->block_bottom : ld->header_h) - origin);
	list_apply_host_extent(ld);
}

void flux_list_set_realized(FluxStore *store, XentNodeId list, int first, int last, int cols) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return;
	flux_list_flow_seed(ld, first, last);
	ld->realized_first = first;
	ld->realized_last  = last;
	ld->stale_fired    = false;
	if (cols > 0 && cols != ld->cols) {
		ld->cols = cols;
		flux_list_relines(ld);
	}
	if (!flux_list_measured(ld)) list_uniform_block(ld);
	flux_list_layout_block(ld);
}

void flux_list_stage_first(FluxStore *store, XentNodeId list, int first) {
	FluxListViewData *ld = list_data(store, list);
	if (ld) ld->realized_first = first > 0 ? first : 0;
}

void flux_list_apply_pending(FluxStore *store, XentNodeId list) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld || ld->host == XENT_NODE_INVALID) return;
	int n = xent_node_nchild(ld->ctx, ld->host) - (ld->header != XENT_NODE_INVALID);
	if (flux_list_grouped(ld) && n > 0 && flux_list_item_of_row(ld, ld->realized_first) >= 0) n--;
	flux_list_set_realized(store, list, ld->realized_first, ld->realized_first + n - 1, list_columns(ld));
}

static void list_scroll_changed(void *ctx) {
	FluxListViewData *ld = ( FluxListViewData * ) ctx;
	FluxNodeData     *nd = flux_store_find(ld->store, ld->root);
	if (nd && flux_list_window_stale(ld->ctx, nd)) flux_list_notify_stale(ld->store, ld->root);
}

static void list_watch_scroll(FluxListViewData *ld) {
	FluxScrollData *sd = flux_list_scroll(ld);
	if (!sd) return;
	sd->on_offset_changed     = list_scroll_changed;
	sd->on_offset_changed_ctx = ld;
}

void flux_list_emitrealize(FluxListViewData *ld) {
	ld->realize_events = true;
	list_watch_scroll(ld);
}

void flux_list_onstale(FluxStore *store, XentNodeId list, void (*on_stale)(void *), void *ctx) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return;
	ld->on_stale     = on_stale;
	ld->on_stale_ctx = ctx;
	list_watch_scroll(ld);
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
	( void ) ctx;
	if (!ld || !(ld->realize_events || ld->on_stale) || ld->stale_fired || ld->count + ld->ngroup <= 0) return false;

	float offset = 0.0f, extent = 0.0f, cross = 0.0f;
	if (!flux_list_viewport(ld->store, ld->root, &offset, &extent, &cross) || extent <= 0.0f) return false;
	if (list_autocolumns(cross, ld->item_width, ld->columns) != ld->cols) return true;

	int visible_first = flux_list_line_first(ld, flux_list_row_at(ld, offset));
	int visible_last  = flux_list_line_last(ld, flux_list_row_at(ld, offset + extent - 0.5f));
	return visible_first < ld->realized_first || visible_last > ld->realized_last;
}

static XtkEventPayload list_window(FluxListViewData const *ld) {
	float offset = 0.0f, extent = 0.0f, cross = 0.0f;
	if (!flux_list_viewport(ld->store, ld->root, &offset, &extent, &cross) || extent <= 0.0f) {
		offset = 0.0f;
		extent = 600.0f;
	}
	int lines = flux_list_lines(ld);
	int first = flux_list_row_at(ld, offset) - 4;
	int last  = flux_list_row_at(ld, offset + extent) + 4;
	if (first < 0) first = 0;
	if (last > lines - 1) last = lines - 1;
	return (XtkEventPayload) {
	  .kind     = XTK_EVENT_PAYLOAD_WINDOW,
	  .u.window = {flux_list_line_first(ld, first), flux_list_line_last(ld, last), ld->cols > 0 ? ld->cols : 1},
	};
}

static void list_sync_cols(FluxListViewData *ld) {
	float offset = 0.0f, extent = 0.0f, cross = 0.0f;
	if (!flux_list_viewport(ld->store, ld->root, &offset, &extent, &cross)) return;
	int cols = list_autocolumns(cross, ld->item_width, ld->columns);
	if (cols == ld->cols) return;
	ld->cols = cols;
	flux_list_relines(ld);
	if (!flux_list_measured(ld)) list_uniform_block(ld);
	flux_list_layout_block(ld);
}

void flux_list_notify_stale(FluxStore *store, XentNodeId list) {
	FluxListViewData *ld   = list_data(store, list);
	XtkHost          *host = flux_store_host(store);
	if (!ld || ld->stale_fired) return;
	list_sync_cols(ld);
	if (ld->on_stale) {
		ld->stale_fired = true;
		ld->on_stale(ld->on_stale_ctx);
		return;
	}
	if (!host || !ld->realize_events) return;
	ld->stale_fired = true;
	XtkEvent event  = {
	  .id             = XTK_EVENT_REALIZE,
	  .target         = ( XtkCtrl ) list,
	  .source         = ( XtkCtrl ) list,
	  .current_target = ( XtkCtrl ) list,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_PROGRAM,
	  .payload        = list_window(ld),
	};
	flux_host_route(host, &event);
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
	if (it->header) {
		xent_seth(owner->ctx, item, NAN);
		xent_setfit(owner->ctx, item, false, true);
		return;
	}
	bool fit = flux_list_measured(owner) && owner->item_height <= 0.0f;
	xent_setfit(owner->ctx, item, false, fit);
	if (fit) {
		xent_seth(owner->ctx, item, NAN);
		xent_setminsize(owner->ctx, item, (XentSize) {0.0f, owner->kind == XTK_LIST_KIND_LIST ? 40.0f : 0.0f});
		return;
	}
	xent_seth(owner->ctx, item, owner->item_height);
	if (owner->item_width > 0.0f) xent_setw(owner->ctx, item, owner->item_width);
}

static void list_item_complete_focus(FluxListItemData *it, XentNodeId item) {
	FluxListViewData *owner = it->owner;
	if (!owner || owner->pending_focus != it->row) return;
	owner->pending_focus = -1;
	if (owner->input) flux_input_setfocus(owner->input, item, false);
}

void fluxlistitem_set_place(FluxStore *store, XentNodeId item, int row, int index, float x, float y) {
	FluxListItemData *it = item_data(store, item);
	if (!it) return;
	fluxlistitem_bind_owner(store, item);
	list_item_set_slot_size(it, item);
	if (it->row != row) it->extent = -1.0f;
	it->row            = row;
	it->index          = it->header ? -1 : index;
	it->group          = it->owner ? flux_list_group_of_row(it->owner, row) : -1;
	it->line           = it->owner ? flux_list_line_of(it->owner, row) : row;
	it->col            = it->owner ? flux_list_col_of(it->owner, row) : 0;
	it->logical_x      = x;
	it->logical_y      = y;
	FluxScrollData *sd = it->owner ? flux_list_scroll(it->owner) : NULL;
	xent_setpos(
	  flux_store_ctx(store), item, (XentPoint) {x - (sd ? sd->origin_x : 0.0f), y - (sd ? sd->origin_y : 0.0f)}
	);
	list_item_complete_focus(it, item);
}

void fluxlistitem_set_state(FluxStore *store, XentNodeId item, bool selected, bool multi) {
	FluxListItemData *it = item_data(store, item);
	if (!it || it->header) return;
	it->selected = selected;
	if (it->multi != multi) {
		it->multi = multi;
		if (it->owner && it->owner->kind == XTK_LIST_KIND_LIST) {
			XentInsets pad
			  = multi ? (XentInsets) {16.0f + 28.0f, 0.0f, 12.0f, 0.0f} : (XentInsets) {16.0f, 0.0f, 12.0f, 0.0f};
			xent_setp(flux_store_ctx(store), item, pad);
		}
	}
}

XentNodeId flux_list_slot(FluxStore *store, XentNodeId list, XentNodeId child) {
	FluxListViewData *ld = list_data(store, list);
	if (!ld) return XENT_NODE_INVALID;
	FluxControlType type = flux_node_type(store, child);
	if (type != FLUX_CONTROL_LIST_ITEM && type != FLUX_CONTROL_GROUP_HEADER && ld->header != child) {
		ld->header   = child;
		ld->header_h = 0.0f;
		ld->header_w = 0.0f;
	}
	return ld->host;
}

static bool list_header_live(FluxListViewData *ld) {
	if (ld->header == XENT_NODE_INVALID) return false;
	if (xent_node_parent(ld->ctx, ld->header) == ld->host) return true;
	ld->header = XENT_NODE_INVALID;
	return false;
}

static void list_header_measure(FluxListViewData *ld, float *h, float *w) {
	XentRect hr = {0}, sr = {0};
	*h = *w = 0.0f;
	if (!list_header_live(ld) || !xent_layout_rect(ld->ctx, ld->header, &hr)) return;
	xent_layout_rect(ld->ctx, ld->scroll, &sr);
	*h = hr.h;
	*w = fmaxf(hr.w, sr.w);
}

bool flux_list_sync_layout(struct FluxNodeData *nd) {
	FluxListViewData *ld = ( FluxListViewData * ) nd->component_data;
	float             h = 0.0f, w = 0.0f;
	if (!ld) return false;
	list_header_measure(ld, &h, &w);
	if (h == ld->header_h && w == ld->header_w) return flux_list_flow_sync(ld);
	ld->header_h = h;
	ld->header_w = w;
	if (w > 0.0f) xent_setw(ld->ctx, ld->host, w);
	else xent_setwpct(ld->ctx, ld->host, 1.0f);
	flux_list_set_realized(ld->store, ld->root, ld->realized_first, ld->realized_last, ld->cols);
	flux_list_flow_sync(ld);
	return true;
}

bool flux_list_uia_selected(FluxStore *store, XentNodeId node) {
	FluxListItemData const *it = item_data(store, node);
	return it && it->owner && !it->header && it->index >= 0 && list_row_selected(it->owner, it->index);
}

static void list_uia_add(FluxListViewData *ld, int index, bool add) {
	if (add == flux_ranges_contains(ld, index)) return;
	if (add) flux_ranges_add(ld, index, index);
	else flux_ranges_remove(ld, index);
	flux_list_report(ld, index);
}

bool flux_list_uia_select(FluxStore *store, XentNodeId node, int op) {
	FluxListItemData const *it = item_data(store, node);
	if (!it || !it->owner || it->header || it->index < 0 || it->owner->sel_mode == XTK_LIST_SELECT_NONE) return false;
	FluxListViewData *ld    = it->owner;
	bool              add   = op != FLUX_UIA_SEL_REMOVE;
	bool              range = multi_mode(ld) && op != FLUX_UIA_SEL_SELECT;
	if (range) list_uia_add(ld, it->index, add);
	if (!range && add) sel_single(ld, it->index);
	if (!range && !add && ld->selected == it->index) list_select_one(ld, -1);
	return true;
}

unsigned flux_list_uia_mode(FluxStore *store, XentNodeId node) {
	FluxListViewData *ld = list_data(store, node);
	return ld && multi_mode(ld) ? FLUX_UIA_SEL_MULTIPLE : 0u;
}
