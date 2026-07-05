/**
 * @file flux_radio_buttons.c
 * @brief RadioButtons grouping control (WinUI RadioButtons.cpp semantics).
 *
 * Lays child radios column-major into a uniform grid (MaxColumns), enforces
 * single selection, and implements the group keyboard model: Up/Down move by
 * index order, Left/Right jump columns, and moving focus SELECTS the newly
 * focused item unless Ctrl is held (selection-follows-focus). One tab stop:
 * the children are the tab targets, the group is a plain container.
 *
 * Layout note: WinUI's ColumnMajorUniformToLargestGridLayout adapts the
 * column count to the available width; fluxent uses the requested MaxColumns
 * statically (clamped to item count) — faithful for the common single-column
 * case and any fixed multi-column group.
 */
#include "controls/factory/flux_factory.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_input.h"

#include <stdlib.h>
#include <windows.h>

#define RB_COLUMN_SPACING 7.0f
#define RB_ROW_SPACING    8.0f
#define RB_HEADER_GAP     8.0f

static FluxRadioButtonsData *rb_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_RADIO_BUTTONS || !nd->component_data) return NULL;
	return ( FluxRadioButtonsData * ) nd->component_data;
}

/* Enforce single selection: check idx, uncheck the rest, notify. */
static void rb_select(FluxRadioButtonsData *g, int idx, bool notify) {
	if (idx < -1 || idx >= g->count || idx == g->selected) return;
	for (int i = 0; i < g->count; i++)
		flux_checkbox_set_state(g->store, g->items [i], i == idx ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED);
	g->selected = idx;
	if (notify && idx >= 0 && g->on_select) g->on_select(g->on_select_ctx, idx);
}

/* Clicking a radio selects it (radios never toggle off via click). */
static void rb_item_click(void *ctx) {
	FluxRadioItemCtx *ic = ( FluxRadioItemCtx * ) ctx;
	rb_select(ic->group, ic->index, true);
}

/* Column-major geometry: column c holds a contiguous run of rows. The first
 * (count % columns) columns get one extra row. Returns the column index and
 * the row within it for a given item, plus the column's row count. */
static void rb_locate(FluxRadioButtonsData const *g, int idx, int *col, int *row, int *col_rows) {
	int cols  = g->columns < 1 ? 1 : (g->columns > g->count ? g->count : g->columns);
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
	*col = 0; *row = idx; *col_rows = g->count;
}

/* First item index of column c (column-major). */
static int rb_col_start(FluxRadioButtonsData const *g, int cols, int c) {
	int base = g->count / cols, extra = g->count % cols, start = 0;
	for (int i = 0; i < c; i++) start += base + (i < extra ? 1 : 0);
	return start;
}

/* Move focus to target (clamped/skip-invalid) and, unless Ctrl, select it. */
static bool rb_focus_move(FluxRadioButtonsData *g, int target) {
	if (target < 0 || target >= g->count) return true; /* edge: consume, no wrap (WinUI) */
	if (g->input) flux_input_set_focus(g->input, g->items [target]);
	if (!(GetKeyState(VK_CONTROL) & 0x8000)) rb_select(g, target, true);
	return true;
}

static bool rb_item_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxRadioItemCtx     *ic = ( FluxRadioItemCtx * ) ctx;
	FluxRadioButtonsData *g  = ic->group;
	int                   idx = ic->index;
	int                   cols = g->columns < 1 ? 1 : (g->columns > g->count ? g->count : g->columns);
	if (cols < 1) cols = 1;

	switch (vk) {
	case VK_DOWN : return rb_focus_move(g, idx + 1);
	case VK_UP   : return rb_focus_move(g, idx - 1);
	case VK_RIGHT : {
		int col, row, col_rows;
		rb_locate(g, idx, &col, &row, &col_rows);
		if (col + 1 >= cols) return true;
		int nstart = rb_col_start(g, cols, col + 1);
		int nrows  = (g->count / cols) + ((col + 1) < (g->count % cols) ? 1 : 0);
		return rb_focus_move(g, nstart + (row < nrows ? row : nrows - 1));
	}
	case VK_LEFT : {
		int col, row, col_rows;
		rb_locate(g, idx, &col, &row, &col_rows);
		if (col == 0) return true;
		int pstart = rb_col_start(g, cols, col - 1);
		return rb_focus_move(g, pstart + row);
	}
	default : return false;
	}
}

/* -------------------------------------------------------------------------
 * Create
 * ---------------------------------------------------------------------- */

static void rb_data_destroy(void *component_data) {
	FluxRadioButtonsData *g = ( FluxRadioButtonsData * ) component_data;
	if (!g) return;
	free(g->items);
	free(g->item_ctx);
	free(g);
}

/* One radio row inside a column: chrome from the plain radio factory, with
 * the group's shared click/key handlers rebound onto the node. */
static void rb_make_item(
  FluxRadioButtonsData *g, FluxRadioButtonsCreateInfo const *info, XentNodeId column, int idx
) {
	char const *label = info->items ? info->items [idx].label : NULL;
	XentNodeId  radio = flux_create_radio(&(FluxToggleCreateInfo) {
	  info->ctx, info->store, column, label, false, NULL, NULL});
	g->items [idx]    = radio;
	g->item_ctx [idx] = (FluxRadioItemCtx) {g, idx};

	FluxNodeData *rnd = flux_node_store_get(info->store, radio);
	if (rnd) {
		rnd->behavior.on_click     = rb_item_click;
		rnd->behavior.on_click_ctx = &g->item_ctx [idx];
		rnd->behavior.on_key       = rb_item_key;
		rnd->behavior.on_key_ctx   = &g->item_ctx [idx];
	}
	if (info->items && info->items [idx].disabled) flux_checkbox_set_enabled(info->store, radio, false);
}

/* Column-major grid: a flex row of per-column flex columns. */
static void rb_build_grid(FluxRadioButtonsData *g, FluxRadioButtonsCreateInfo const *info, XentNodeId root, int count) {
	int cols = g->columns > count ? count : g->columns;
	if (cols < 1) cols = 1;
	XentNodeId grid = flux_factory_create_node(info->ctx, info->store, root, FLUX_CONTROL_CONTAINER);
	xent_set_protocol(info->ctx, grid, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, grid, XENT_FLEX_ROW);
	xent_set_flex_align_items(info->ctx, grid, XENT_FLEX_ALIGN_START);
	xent_set_gap(info->ctx, grid, RB_COLUMN_SPACING);

	int base = count / cols, extra = count % cols, idx = 0;
	for (int c = 0; c < cols; c++) {
		XentNodeId column = flux_factory_create_node(info->ctx, info->store, grid, FLUX_CONTROL_CONTAINER);
		xent_set_protocol(info->ctx, column, XENT_PROTOCOL_FLEX);
		xent_set_flex_direction(info->ctx, column, XENT_FLEX_COLUMN);
		xent_set_flex_align_items(info->ctx, column, XENT_FLEX_ALIGN_START);
		xent_set_gap(info->ctx, column, RB_ROW_SPACING);
		int len = base + (c < extra ? 1 : 0);
		for (int r = 0; r < len && idx < count; r++, idx++) rb_make_item(g, info, column, idx);
	}
}

XentNodeId flux_create_radio_buttons(FluxRadioButtonsCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	int count = info->item_count < 0 ? 0 : info->item_count;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_RADIO_BUTTONS);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData         *nd = flux_node_store_get(info->store, root);
	FluxRadioButtonsData *g  = nd ? ( FluxRadioButtonsData * ) calloc(1, sizeof(*g)) : NULL;
	XentNodeId           *items = count ? ( XentNodeId * ) calloc(( size_t ) count, sizeof(XentNodeId)) : NULL;
	FluxRadioItemCtx     *ictx  = count ? ( FluxRadioItemCtx * ) calloc(( size_t ) count, sizeof(FluxRadioItemCtx)) : NULL;
	if (!nd || !g || (count && (!items || !ictx))) {
		free(g); free(items); free(ictx);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	g->ctx           = info->ctx;
	g->store         = info->store;
	g->input         = info->input;
	g->root          = root;
	g->count         = count;
	g->selected      = -1;
	g->columns       = info->max_columns > 0 ? info->max_columns : 1;
	g->items         = items;
	g->item_ctx      = ictx;
	g->on_select     = info->on_select;
	g->on_select_ctx = info->userdata;

	nd->component_data         = g;
	nd->destroy_component_data = rb_data_destroy;

	xent_set_protocol(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, root, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(info->ctx, root, XENT_FLEX_ALIGN_START);
	if (info->header && info->header [0]) xent_set_gap(info->ctx, root, RB_HEADER_GAP);
	xent_set_semantic_role(info->ctx, root, XENT_SEMANTIC_CONTAINER);

	if (info->header && info->header [0])
		flux_create_text(&(FluxTextCreateInfo) {info->ctx, info->store, root, info->header, 14.0f});

	rb_build_grid(g, info, root, count);

	if (info->selected >= 0 && info->selected < count) rb_select(g, info->selected, false);
	if (info->disabled) {
		g->disabled = true;
		for (int i = 0; i < count; i++) flux_checkbox_set_enabled(info->store, g->items [i], false);
	}
	return root;
}

void flux_radio_buttons_set_selected(FluxNodeStore *store, XentNodeId id, int index) {
	FluxRadioButtonsData *g = rb_data(store, id);
	if (g) rb_select(g, index, false);
}

void flux_radio_buttons_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
	FluxRadioButtonsData *g = rb_data(store, id);
	if (!g) return;
	g->disabled = !enabled;
	for (int i = 0; i < g->count; i++) flux_checkbox_set_enabled(store, g->items [i], enabled);
}
