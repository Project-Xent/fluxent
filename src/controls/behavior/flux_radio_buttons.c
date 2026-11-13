#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "controls/factory/flux_init_basic.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_input.h"

#include <stdlib.h>
#include <windows.h>
#include <xtk/xtk_catalog.h>

#define RB_COLUMN_SPACING 7.0f
#define RB_ROW_SPACING    8.0f
#define RB_HEADER_GAP     8.0f

static FluxRadioButtonsData *rb_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_RADIO_BUTTONS || !nd->component_data) return NULL;
	return ( FluxRadioButtonsData * ) nd->component_data;
}

static void rb_apply_checks(FluxRadioButtonsData *g, int idx) {
	for (int i = 0; i < g->count; i++)
		flux_prop_checkbox_state(g->store, g->items [i], i == idx ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED);
}

static void rb_select(FluxRadioButtonsData *g, int idx, bool notify) {
	if (idx < -1 || idx >= g->count || idx == g->selected) return;
	rb_apply_checks(g, idx);
	g->selected = idx;
	if (!notify || idx < 0) return;
	void     (*on_select)(void *, int) = g->on_select;
	void    *on_select_ctx             = g->on_select_ctx;
	XtkHost *host                      = flux_store_host(g->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_SELECT,
		  .target         = ( XtkCtrl ) g->root,
		  .source         = ( XtkCtrl ) g->root,
		  .current_target = ( XtkCtrl ) g->root,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = XTK_EVENT_CAUSE_USER,
		  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = idx},
		};
		flux_host_route(host, &event);
	}
	if (on_select) on_select(on_select_ctx, idx);
}

static void rb_item_click(void *ctx) {
	FluxRadioItemCtx *ic = ( FluxRadioItemCtx * ) ctx;
	rb_select(ic->group, ic->index, true);
}

static void rb_locate(FluxRadioButtonsData const *g, int idx, int *col, int *row, int *col_rows) {
	int cols = g->columns < 1 ? 1 : (g->columns > g->count ? g->count : g->columns);
	if (cols < 1) cols = 1;
	int base  = g->count / cols;
	int extra = g->count % cols;
	int start = 0, c = 0;
	for (; c < cols; c++) {
		int len = base + (c < extra ? 1 : 0);
		if (idx < start + len) {
			*col      = c;
			*row      = idx - start;
			*col_rows = len;
			return;
		}
		start += len;
	}
	*col      = 0;
	*row      = idx;
	*col_rows = g->count;
}

static int rb_col_start(FluxRadioButtonsData const *g, int cols, int c) {
	int base = g->count / cols, extra = g->count % cols, start = 0;
	for (int i = 0; i < c; i++) start += base + (i < extra ? 1 : 0);
	return start;
}

static bool rb_focus_move(FluxRadioButtonsData *g, int target) {
	if (target < 0 || target >= g->count) return true;
	FluxStore *store = g->store;
	XentNodeId root  = g->root;
	if (g->input) flux_input_setfocus(g->input, g->items [target], false);
	g = rb_data(store, root);
	if (!g) return true;
	if (!flux_key_held(FLUX_KEY_CONTROL)) rb_select(g, target, true);
	return true;
}

static int rb_column_count(FluxRadioButtonsData const *g) {
	int cols = g->columns < 1 ? 1 : (g->columns > g->count ? g->count : g->columns);
	return cols < 1 ? 1 : cols;
}

static int rb_horizontal_target(FluxRadioButtonsData const *g, int index, int direction) {
	int cols = rb_column_count(g);
	int col, row, col_rows;
	rb_locate(g, index, &col, &row, &col_rows);
	int next_col = col + direction;
	if (next_col < 0 || next_col >= cols) return index;
	int start = rb_col_start(g, cols, next_col);
	int rows  = (g->count / cols) + (next_col < (g->count % cols) ? 1 : 0);
	if (row >= rows) row = rows - 1;
	return start + row;
}

static bool rb_focus_horizontal(FluxRadioButtonsData *g, int index, int direction) {
	int target = rb_horizontal_target(g, index, direction);
	return target == index || rb_focus_move(g, target);
}

static bool rb_item_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxRadioItemCtx     *item = ( FluxRadioItemCtx * ) ctx;
	FluxRadioButtonsData *g    = item->group;
	if (key == FLUX_KEY_DOWN) return rb_focus_move(g, item->index + 1);
	if (key == FLUX_KEY_UP) return rb_focus_move(g, item->index - 1);
	if (key == FLUX_KEY_RIGHT) return rb_focus_horizontal(g, item->index, 1);
	if (key == FLUX_KEY_LEFT) return rb_focus_horizontal(g, item->index, -1);
	return false;
}

static void rb_data_destroy(void *component_data) {
	FluxRadioButtonsData *g = ( FluxRadioButtonsData * ) component_data;
	if (!g) return;
	free(g->items);
	free(g->item_ctx);
	free(g);
}

static void rb_make_item(FluxRadioButtonsData *g, FluxRadioButtonsDesc const *info, XentNodeId column, int idx) {
	char const *label = info->items ? info->items [idx].label : NULL;
	XentNodeId  radio = flux_radio_init(&(FluxToggleDesc) {info->ctx, info->store, column, label, false, NULL, NULL});
	g->items [idx]    = radio;
	g->item_ctx [idx] = (FluxRadioItemCtx) {g, idx};

	FluxNodeData *rnd = flux_store_find(info->store, radio);
	if (rnd) {
		rnd->behavior.on_click     = rb_item_click;
		rnd->behavior.on_click_ctx = &g->item_ctx [idx];
		rnd->behavior.on_key       = rb_item_key;
		rnd->behavior.on_key_ctx   = &g->item_ctx [idx];
	}
	if (info->items && info->items [idx].disabled) flux_prop_checkbox_enabled(info->store, radio, false);
}

static void rb_build_grid(FluxRadioButtonsData *g, FluxRadioButtonsDesc const *info, XentNodeId root, int count) {
	int cols = g->columns > count ? count : g->columns;
	if (cols < 1) cols = 1;
	XentNodeId grid = flux_factory_create_node(info->ctx, info->store, root, FLUX_CONTROL_CONTAINER);
	xent_setproto(info->ctx, grid, XENT_PROTOCOL_FLEX);
	xent_setflexdir(info->ctx, grid, XENT_FLEX_ROW);
	xent_setitems(info->ctx, grid, XENT_FLEX_ALIGN_START);
	xent_setgap(info->ctx, grid, RB_COLUMN_SPACING);

	int base = count / cols, extra = count % cols, idx = 0;
	for (int c = 0; c < cols; c++) {
		XentNodeId column = flux_factory_create_node(info->ctx, info->store, grid, FLUX_CONTROL_CONTAINER);
		xent_setproto(info->ctx, column, XENT_PROTOCOL_FLEX);
		xent_setflexdir(info->ctx, column, XENT_FLEX_COLUMN);
		xent_setitems(info->ctx, column, XENT_FLEX_ALIGN_START);
		xent_setgap(info->ctx, column, RB_ROW_SPACING);
		int len = base + (c < extra ? 1 : 0);
		for (int r = 0; r < len && idx < count; r++, idx++) rb_make_item(g, info, column, idx);
	}
}

static FluxRadioButtonsData *
rb_runtime_create(FluxRadioButtonsDesc const *info, FluxNodeData *nd, XentNodeId root, int count) {
	FluxRadioButtonsData *g        = ( FluxRadioButtonsData * ) calloc(1, sizeof(*g));
	XentNodeId           *items    = count ? ( XentNodeId * ) calloc(( size_t ) count, sizeof(*items)) : NULL;
	FluxRadioItemCtx     *item_ctx = count ? ( FluxRadioItemCtx * ) calloc(( size_t ) count, sizeof(*item_ctx)) : NULL;
	if (!g || (count && (!items || !item_ctx))) {
		free(g);
		free(items);
		free(item_ctx);
		return NULL;
	}
	*g = (FluxRadioButtonsData) {
	  .ctx           = info->ctx,
	  .store         = info->store,
	  .input         = info->input,
	  .root          = root,
	  .count         = count,
	  .selected      = -1,
	  .columns       = info->max_columns > 0 ? info->max_columns : 1,
	  .items         = items,
	  .item_ctx      = item_ctx,
	  .on_select     = info->on_select,
	  .on_select_ctx = info->userdata,
	};
	nd->component_data         = g;
	nd->destroy_component_data = rb_data_destroy;
	return g;
}

static void rb_setup_root(FluxRadioButtonsDesc const *info, XentNodeId root) {
	xent_setproto(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_setflexdir(info->ctx, root, XENT_FLEX_COLUMN);
	xent_setitems(info->ctx, root, XENT_FLEX_ALIGN_START);
	if (info->header && info->header [0]) xent_setgap(info->ctx, root, RB_HEADER_GAP);
	xent_sem_setrole(info->ctx, root, XENT_SEM_ROLE_CONTAINER);
	if (info->header && info->header [0])
		flux_text_init(&(FluxTextDesc) {info->ctx, info->store, root, info->header, 14.0f});
}

static void rb_apply_initial_state(FluxRadioButtonsData *group, FluxRadioButtonsDesc const *info, int count) {
	if (info->selected >= 0 && info->selected < count) rb_select(group, info->selected, false);
	if (!info->disabled) return;
	group->disabled = true;
	for (int i = 0; i < count; i++) flux_prop_checkbox_enabled(info->store, group->items [i], false);
}

XentNodeId flux_radiogroup_init(FluxRadioButtonsDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	int        count = info->item_count < 0 ? 0 : info->item_count;

	XentNodeId root  = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_RADIO_BUTTONS);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData         *nd = flux_store_find(info->store, root);
	FluxRadioButtonsData *g  = nd ? rb_runtime_create(info, nd, root, count) : NULL;
	if (!g) return flux_factory_fail_node(info->ctx, info->store, root);
	rb_setup_root(info, root);
	rb_build_grid(g, info, root, count);
	rb_apply_initial_state(g, info, count);
	return root;
}

void flux_radiogroup_set_selected(FluxStore *store, XentNodeId id, int index) {
	FluxRadioButtonsData *g = rb_data(store, id);
	if (!g) return;
	if (g->count <= 0) {
		g->selected = index;
		return;
	}
	rb_select(g, index, false);
}

void flux_radiogroup_set_enabled(FluxStore *store, XentNodeId id, bool enabled) {
	FluxRadioButtonsData *g = rb_data(store, id);
	if (!g) return;
	g->disabled = !enabled;
	for (int i = 0; i < g->count; i++) flux_prop_checkbox_enabled(store, g->items [i], enabled);
}

static int rb_public_child_count(FluxRadioButtonsData const *g, FluxStore *store) {
	int count = 0;
	for (XentNodeId child = xent_node_first(g->ctx, g->root); child != XENT_NODE_INVALID;
	  child               = xent_node_next(g->ctx, child))
		if (flux_node_type(store, child) == FLUX_CONTROL_RADIO) count++;
	return count;
}

static bool rb_resize_items(FluxRadioButtonsData *g, int count) {
	if (count == g->count) return true;
	XentNodeId       *items    = count ? ( XentNodeId * ) calloc(( size_t ) count, sizeof(*items)) : NULL;
	FluxRadioItemCtx *item_ctx = count ? ( FluxRadioItemCtx * ) calloc(( size_t ) count, sizeof(*item_ctx)) : NULL;
	if (count && (!items || !item_ctx)) {
		free(items);
		free(item_ctx);
		return false;
	}
	free(g->items);
	free(g->item_ctx);
	g->items    = items;
	g->item_ctx = item_ctx;
	g->count    = count;
	if (g->selected >= count) g->selected = -1;
	return true;
}

static void rb_bind_public_children(FluxRadioButtonsData *g, FluxStore *store) {
	int idx = 0;
	for (XentNodeId child = xent_node_first(g->ctx, g->root); child != XENT_NODE_INVALID;
	  child               = xent_node_next(g->ctx, child))
	{
		if (flux_node_type(store, child) != FLUX_CONTROL_RADIO) continue;
		g->items [idx]    = child;
		g->item_ctx [idx] = (FluxRadioItemCtx) {g, idx};
		FluxNodeData *nd  = flux_store_find(store, child);
		if (nd) {
			nd->behavior.on_click     = rb_item_click;
			nd->behavior.on_click_ctx = &g->item_ctx [idx];
			nd->behavior.on_key       = rb_item_key;
			nd->behavior.on_key_ctx   = &g->item_ctx [idx];
		}
		idx++;
	}
}

void flux_radiogroup_sync_children(FluxStore *store, XentNodeId id) {
	FluxRadioButtonsData *g = rb_data(store, id);
	if (!g) return;
	xent_setgap(g->ctx, g->root, RB_ROW_SPACING);
	if (!rb_resize_items(g, rb_public_child_count(g, store))) return;
	rb_bind_public_children(g, store);
	rb_apply_checks(g, g->selected);
}
