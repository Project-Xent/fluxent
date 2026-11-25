#include "controls/behavior/flux_uia_part.h"
#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"

#include <string.h>

XentNodeId flux_uia_part(XentCtx *ctx, FluxStore *store, XentNodeId owner, XentNodeId parent, int index) {
	XentNodeId    n  = flux_factory_create_node(ctx, store, parent, FLUX_CONTROL_CONTAINER);
	FluxNodeData *nd = n != XENT_NODE_INVALID ? flux_store_find(store, n) : NULL;
	if (!nd) return XENT_NODE_INVALID;
	nd->uia_owner = owner;
	nd->uia_part  = index;
	xent_setproto(ctx, parent, XENT_PROTOCOL_ABSOLUTE);
	xent_setproto(ctx, n, XENT_PROTOCOL_ABSOLUTE);
	xent_setcollapsed(ctx, n, true);
	return n;
}

void flux_uia_part_hooks(FluxStore *store, XentNodeId part, struct FluxUiaHooks const *uia) {
	FluxNodeData *nd = flux_store_find(store, part);
	if (nd) nd->uia = uia;
}

void flux_uia_part_place(XentCtx *ctx, XentNodeId part, XentRect local, XentPoint pad) {
	bool shown = local.w > 0.0f && local.h > 0.0f;
	xent_setcollapsed(ctx, part, !shown);
	if (!shown) return;
	xent_setpos(ctx, part, (XentPoint) {local.x - pad.x, local.y - pad.y});
	xent_setsize(ctx, part, (XentSize) {local.w, local.h});
}

void flux_uia_part_label(XentCtx *ctx, XentNodeId part, char const *name, bool enabled) {
	char const *now = xent_sem_label(ctx, part);
	if (!now || !name || strcmp(now, name) != 0) xent_sem_setlabel(ctx, part, name ? name : "");
	if (xent_sem_enabled(ctx, part) != enabled) xent_sem_setenabled(ctx, part, enabled);
}

XentNodeId flux_uia_part_owner(FluxStore *store, XentNodeId part, int *index) {
	FluxNodeData const *nd = flux_store_find(store, part);
	if (!nd || nd->popup || nd->uia_owner == XENT_NODE_INVALID) return XENT_NODE_INVALID;
	if (index) *index = nd->uia_part;
	return nd->uia_owner;
}
