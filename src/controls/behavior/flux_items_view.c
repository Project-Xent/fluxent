/**
 * @file flux_items_view.c
 * @brief ItemsView items-host root: a thin configuration adapter over the
 * virtualized ListView spine (flux_list_view.c).
 *
 * ItemsView reuses the entire items-host machinery — scroll viewport, the
 * ABSOLUTE recycled-cell host, the reconciler window, and the WinUI
 * SelectionModel (None/Single/Multiple/Extended). This file only:
 *   - builds the same root -> SCROLL -> ABSOLUTE host spine, tagged
 *     FLUX_CONTROL_ITEMS_VIEW and stamped with the ItemContainer chrome kind
 *     (XTK_LIST_KIND_ITEMS);
 *   - stores the FluxItemsViewData (a FluxListViewData embedded first, so the
 *     shared list accessors operate on it verbatim);
 *   - forwards its public setters to the flux_list_view_* API.
 *
 * Selection, keyboard navigation, ItemInvoked firing and cell recycling are
 * all handled by the shared LIST_ITEM behavior — ItemInvoked rides on the
 * FluxListViewData::on_invoke slot the shared release/Enter path fires when
 * SelectionMode == None (WinUI ItemsView.IsItemInvokedEnabled gating).
 */
#include "controls/factory/flux_factory.h"
#include "fluxent/controls/flux_items_view_data.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_input.h"

#include <stdlib.h>

static void items_view_data_destroy(void *component_data) {
	FluxItemsViewData *d = ( FluxItemsViewData * ) component_data;
	if (!d) return;
	free(d->list.ranges); /* embedded spine owns the Multiple/Extended range set */
	free(d);
}

XentNodeId flux_create_items_view(FluxItemsViewCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_ITEMS_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData      *nd = flux_node_store_get(info->store, root);
	FluxItemsViewData *d  = nd ? ( FluxItemsViewData * ) calloc(1, sizeof(FluxItemsViewData)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}
	FluxListViewData *ld = &d->list;

	xent_set_protocol(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, root, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);

	XentNodeId scroll = flux_create_scroll(&(FluxContainerCreateInfo) {info->ctx, info->store, root});
	XentNodeId host   = scroll != XENT_NODE_INVALID
	    ? flux_factory_create_node(info->ctx, info->store, scroll, FLUX_CONTROL_CONTAINER)
	    : XENT_NODE_INVALID;
	if (host == XENT_NODE_INVALID) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	/* Identical layout contract to flux_create_list_view: the scroll fills the
	 * root, the host is the ABSOLUTE scrolled canvas (rows x item_height) and
	 * must never flex-shrink to the viewport. */
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
	ld->kind                 = ( XtkListKind ) XTK_LIST_KIND_ITEMS; /* ItemContainer chrome + interactive cells */
	ld->sel_mode             = info->selection_mode;
	ld->cols                 = 1;
	ld->selected             = -1;
	ld->realized_last        = -1;
	ld->focused              = -1;
	ld->pending_focus        = -1;
	ld->single_follows_focus = true;
	ld->on_select            = info->on_select;
	ld->on_select_ctx        = info->userdata;
	ld->item_invoked_enabled = info->item_invoked_enabled;
	ld->on_invoke            = info->on_invoke;
	ld->on_invoke_ctx        = info->userdata;

	d->layout       = info->layout;
	d->item_spacing = info->item_spacing;

	nd->component_data         = d;
	nd->destroy_component_data = items_view_data_destroy;

	xent_set_semantic_role(info->ctx, root, XENT_SEMANTIC_CONTAINER);
	return root;
}

/* -------------------------------------------------------------------------
 * Store-facing setters — thin forwards onto the shared items-host API. The
 * FluxListViewData embedded at offset 0 means these operate on the same
 * retained spine the ListView family uses; the only requirement is that the
 * shared accessors admit FLUX_CONTROL_ITEMS_VIEW (see WIRING: list_data()).
 * ---------------------------------------------------------------------- */

XentNodeId flux_items_view_content_node(FluxNodeStore *store, XentNodeId view) {
	return flux_list_view_content_node(store, view);
}

void flux_items_view_set_extent(
  FluxNodeStore *store, XentNodeId view, int count, float item_height, float item_width, int columns
) {
	flux_list_view_set_extent(store, view, count, item_height, item_width, columns);
}

void flux_items_view_set_selection_mode(FluxNodeStore *store, XentNodeId view, XtkListSelMode mode) {
	flux_list_view_set_sel_mode(store, view, mode);
}

void flux_items_view_set_selected(FluxNodeStore *store, XentNodeId view, int index) {
	flux_list_view_set_selected(store, view, index);
}

bool flux_items_view_is_selected(FluxNodeStore *store, XentNodeId view, int index) {
	return flux_list_view_is_selected(store, view, index);
}

int flux_items_view_selection_count(FluxNodeStore *store, XentNodeId view) {
	return flux_list_view_selection_count(store, view);
}
