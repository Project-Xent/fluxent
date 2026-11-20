#include "a11y/flux_uia_tree.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_module.h"

typedef XentNodeId (*UiaStep)(XentCtx const *ctx, XentNodeId node);

static bool        uia_hidden(FluxUiaContext const *info, XentNodeId node) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	return h && h->role == FLUX_UIA_HIDDEN;
}

bool flux_uia_exposed(FluxUiaContext const *info, XentNodeId node) {
	if (node == info->root) return true;
	return !xent_collapsed(info->ctx, node) && !uia_hidden(info, node);
}

XentNodeId flux_uia_parent(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId p = xent_node_parent(info->ctx, node);
	while (p != XENT_NODE_INVALID && p != info->root && !flux_uia_exposed(info, p)) p = xent_node_parent(info->ctx, p);
	return p;
}

XentNodeId flux_uia_visible(FluxUiaContext const *info, XentNodeId node) {
	return node == XENT_NODE_INVALID || flux_uia_exposed(info, node) ? node : flux_uia_parent(info, node);
}

static XentNodeId uia_edge(FluxUiaContext const *info, XentNodeId node, UiaStep start, UiaStep step) {
	for (XentNodeId c = start(info->ctx, node); c != XENT_NODE_INVALID; c = step(info->ctx, c)) {
		if (xent_collapsed(info->ctx, c)) continue;
		if (!uia_hidden(info, c)) return c;
		XentNodeId inner = uia_edge(info, c, start, step);
		if (inner != XENT_NODE_INVALID) return inner;
	}
	return XENT_NODE_INVALID;
}

XentNodeId flux_uia_first(FluxUiaContext const *info, XentNodeId node) {
	return uia_edge(info, node, xent_node_first, xent_node_next);
}

XentNodeId flux_uia_last(FluxUiaContext const *info, XentNodeId node) {
	return uia_edge(info, node, xent_node_last, xent_node_prev);
}

static XentNodeId uia_sibling(FluxUiaContext const *info, XentNodeId node, UiaStep step, UiaStep inner_start) {
	for (XentNodeId s = step(info->ctx, node); s != XENT_NODE_INVALID; s = step(info->ctx, s)) {
		if (xent_collapsed(info->ctx, s)) continue;
		if (!uia_hidden(info, s)) return s;
		XentNodeId inner = uia_edge(info, s, inner_start, step);
		if (inner != XENT_NODE_INVALID) return inner;
	}
	XentNodeId p = xent_node_parent(info->ctx, node);
	if (p == XENT_NODE_INVALID || p == info->root || flux_uia_exposed(info, p)) return XENT_NODE_INVALID;
	return uia_sibling(info, p, step, inner_start);
}

XentNodeId flux_uia_next(FluxUiaContext const *info, XentNodeId node) {
	return uia_sibling(info, node, xent_node_next, xent_node_first);
}

XentNodeId flux_uia_prev(FluxUiaContext const *info, XentNodeId node) {
	return uia_sibling(info, node, xent_node_prev, xent_node_last);
}
