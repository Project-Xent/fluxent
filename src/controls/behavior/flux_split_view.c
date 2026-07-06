/**
 * @file flux_split_view.c
 * @brief SplitView behavior: an ABSOLUTE root hosting a content wrapper (first)
 * and a pane wrapper (second, drawn on top for overlay modes). A per-frame sync
 * positions the two wrappers per display mode / placement / open state and
 * refreshes the pane wrapper's render data (overlay + divider edge).
 */
#include "controls/factory/flux_factory.h"
#include "fluxent/fluxent.h"

#include <stdlib.h>

static FluxSplitViewData *split_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_SPLIT_VIEW || !nd->component_data) return NULL;
	return ( FluxSplitViewData * ) nd->component_data;
}

/* Configure a wrapper as a FLEX column that stretches its single child. */
static void split_wrap_layout(XentContext *ctx, XentNodeId n) {
	xent_set_protocol(ctx, n, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(ctx, n, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(ctx, n, XENT_FLEX_ALIGN_STRETCH);
}

XentNodeId flux_create_split_view_content(XentContext *ctx, FluxNodeStore *store, XentNodeId parent) {
	XentNodeId n = flux_factory_create_node(ctx, store, parent, FLUX_CONTROL_SPLIT_VIEW_CONTENT);
	if (n != XENT_NODE_INVALID) split_wrap_layout(ctx, n);
	return n;
}

XentNodeId flux_create_split_view_pane(XentContext *ctx, FluxNodeStore *store, XentNodeId parent) {
	XentNodeId n = flux_factory_create_node(ctx, store, parent, FLUX_CONTROL_SPLIT_VIEW_PANE);
	if (n == XENT_NODE_INVALID) return n;

	FluxNodeData      *nd = flux_node_store_get(store, n);
	FluxSplitPaneData *pd = nd ? ( FluxSplitPaneData * ) calloc(1, sizeof(*pd)) : NULL;
	if (!nd || !pd) {
		free(pd);
		return flux_factory_fail_node(ctx, store, n);
	}
	nd->component_data         = pd;
	nd->destroy_component_data = free;
	nd->clips_children         = true; /* clip a partially-open (compact) pane */
	split_wrap_layout(ctx, n);
	return n;
}

XentNodeId flux_create_split_view(FluxSplitViewCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SPLIT_VIEW);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData      *nd = flux_node_store_get(info->store, node);
	FluxSplitViewData *d  = nd ? ( FluxSplitViewData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	d->ctx          = info->ctx;
	d->store        = info->store;
	d->node         = node;
	d->display_mode = ( uint8_t ) info->display_mode;
	d->placement    = ( uint8_t ) info->pane_placement;
	d->pane_open    = info->pane_open;
	d->open_len     = info->open_pane_length > 0.0f ? info->open_pane_length : FLUX_SPLITVIEW_OPEN_LEN;
	d->compact_len  = info->compact_pane_length > 0.0f ? info->compact_pane_length : FLUX_SPLITVIEW_COMPACT_LEN;

	nd->component_data         = d;
	nd->destroy_component_data = free;
	nd->clips_children         = true;

	xent_set_protocol(info->ctx, node, XENT_PROTOCOL_ABSOLUTE);
	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_CONTAINER);
	return node;
}

void flux_split_view_sync(XentContext *ctx, XentNodeId node, struct FluxNodeData *nd) {
	FluxSplitViewData *d = ( FluxSplitViewData * ) nd->component_data;
	XentRect           r = {0};
	if (!d || !xent_get_layout_rect(ctx, node, &r) || r.w <= 0.0f || r.h <= 0.0f) return;

	XentNodeId content = xent_get_first_child(ctx, node);
	if (content == XENT_NODE_INVALID) return;
	XentNodeId pane = xent_get_next_sibling(ctx, content);

	bool  compact = flux_splitview_is_compact(d->display_mode);
	bool  overlay = flux_splitview_is_overlay(d->display_mode);
	float closed  = compact ? d->compact_len : 0.0f;
	float pane_w  = d->pane_open ? d->open_len : closed;
	if (pane_w > r.w) pane_w = r.w;

	float content_x, content_w, pane_x;
	if (d->placement == FLUX_SPLITVIEW_RIGHT) {
		float inset = overlay ? closed : pane_w;
		content_x   = 0.0f;
		content_w   = r.w - inset;
		pane_x      = r.w - pane_w;
	}
	else {
		float inset = overlay ? closed : pane_w;
		content_x   = inset;
		content_w   = r.w - inset;
		pane_x      = 0.0f;
	}
	if (content_w < 0.0f) content_w = 0.0f;

	xent_set_absolute_position(ctx, content, (XentPoint) {content_x, 0.0f});
	xent_set_size(ctx, content, (XentSize) {content_w, r.h});

	if (pane != XENT_NODE_INVALID) {
		xent_set_absolute_position(ctx, pane, (XentPoint) {pane_x, 0.0f});
		xent_set_size(ctx, pane, (XentSize) {pane_w, r.h});

		FluxNodeData *pnd = flux_node_store_get(d->store, pane);
		if (pnd && pnd->component_data) {
			FluxSplitPaneData *pd = ( FluxSplitPaneData * ) pnd->component_data;
			pd->overlay           = overlay;
			pd->placement         = d->placement;
			pd->divider           = pane_w > 0.5f;
		}
	}
}

void flux_split_view_set_pane_open(FluxNodeStore *store, XentNodeId id, bool open) {
	FluxSplitViewData *d = split_data(store, id);
	if (d) d->pane_open = open;
}

void flux_split_view_configure(
  FluxNodeStore *store, XentNodeId id, int display_mode, int placement, float open_len, float compact_len
) {
	FluxSplitViewData *d = split_data(store, id);
	if (!d) return;
	d->display_mode = ( uint8_t ) display_mode;
	d->placement    = ( uint8_t ) placement;
	if (open_len > 0.0f) d->open_len = open_len;
	if (compact_len > 0.0f) d->compact_len = compact_len;
}
