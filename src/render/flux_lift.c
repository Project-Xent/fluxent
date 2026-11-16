#include "render/flux_lift.h"
#include "controls/data/flux_scroll_data.h"

static void lift_shift(FluxStore *store, XentNodeId node, FluxLiftPlace *place) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd) return;
	place->dx += nd->render_translate_x;
	place->dy += nd->render_translate_y;
	if (nd->component_type != FLUX_CONTROL_SCROLL || !nd->component_data) return;
	FluxScrollData const *sd  = ( FluxScrollData const * ) nd->component_data;
	place->dx                -= flux_scroll_off_x(sd);
	place->dy                -= flux_scroll_off_y(sd);
}

bool flux_lift_place(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxLiftPlace *out) {
	FluxLiftPlace place = {.layer = node};
	XentNodeId    p     = xent_node_parent(ctx, node);
	if (p == XENT_NODE_INVALID || xent_node_parent(ctx, p) == XENT_NODE_INVALID) return false;
	for (; xent_node_parent(ctx, p) != XENT_NODE_INVALID; p = xent_node_parent(ctx, p)) {
		lift_shift(store, p, &place);
		place.layer = p;
	}
	*out = place;
	return true;
}

bool flux_lift_within(XentCtx const *ctx, FluxStore const *store, XentNodeId node) {
	if (!flux_store_nlifted(store)) return false;
	for (; node != XENT_NODE_INVALID; node = xent_node_parent(ctx, node))
		if (flux_node_lifted(store, node)) return true;
	return false;
}
