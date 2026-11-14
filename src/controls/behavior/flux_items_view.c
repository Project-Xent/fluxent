#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "controls/data/flux_items_view_data.h"
#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_items.h"
#include "fluxent/flux_input.h"

#include <stdlib.h>

static void items_view_data_destroy(void *component_data) {
	FluxItemsViewData *d = ( FluxItemsViewData * ) component_data;
	if (!d) return;
	free(d->list.ranges);
	free(d);
}

XentNodeId flux_itemsview_init(FluxItemsViewDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_ITEMS_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData      *nd = flux_store_find(info->store, root);
	FluxItemsViewData *d  = nd ? ( FluxItemsViewData * ) calloc(1, sizeof(FluxItemsViewData)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}
	FluxListViewData *ld = &d->list;

	xent_setproto(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_setflexdir(info->ctx, root, XENT_FLEX_COLUMN);
	xent_setitems(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);

	XentNodeId scroll = flux_scroll_init(&(FluxContainerDesc) {info->ctx, info->store, root});
	XentNodeId host   = scroll != XENT_NODE_INVALID
	                    ? flux_factory_create_node(info->ctx, info->store, scroll, FLUX_CONTROL_CONTAINER)
	                    : XENT_NODE_INVALID;
	if (host == XENT_NODE_INVALID) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	xent_setgrow(info->ctx, scroll, 1.0f);
	xent_setbasis(info->ctx, scroll, 0.0f);
	xent_setproto(info->ctx, host, XENT_PROTOCOL_ABSOLUTE);
	xent_setwpct(info->ctx, host, 1.0f);
	xent_seth(info->ctx, host, 0.0f);
	xent_setshrink(info->ctx, host, 0.0f);

	ld->ctx                  = info->ctx;
	ld->store                = info->store;
	ld->input                = info->input;
	ld->root                 = root;
	ld->scroll               = scroll;
	ld->host                 = host;
	ld->kind                 = ( XtkListKind ) XTK_LIST_KIND_ITEMS;
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
	flux_list_emitrealize(ld);

	d->layout                  = info->layout;
	d->item_spacing            = info->item_spacing;

	nd->component_data         = d;
	nd->destroy_component_data = items_view_data_destroy;

	xent_sem_setrole(info->ctx, root, XENT_SEM_ROLE_CONTAINER);
	return root;
}

XentNodeId flux_itemsview_content_node(FluxStore *store, XentNodeId view) {
	return flux_list_content_node(store, view);
}

void flux_itemsview_set_extent(
  FluxStore *store, XentNodeId view, int count, float item_height, float item_width, int columns
) {
	flux_list_set_extent(store, view, count, item_height, item_width, columns);
}
