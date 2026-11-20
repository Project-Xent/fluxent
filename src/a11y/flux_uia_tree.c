#include "a11y/flux_uia_tree.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_module.h"
#include "store/flux_node_data.h"

#include <fluxent/flux_popup.h>

typedef XentNodeId (*UiaStep)(FluxUiaContext const *info, XentNodeId node);

static XentNodeId  uia_owner_of(FluxUiaContext const *info, XentNodeId node) {
	FluxNodeData const *nd = flux_store_find(info->store, node);
	return nd && nd->popup ? nd->uia_owner : XENT_NODE_INVALID;
}

static XentNodeId uia_popup_of(FluxUiaContext const *info, XentNodeId owner) {
	FluxNodeData const *nd   = flux_store_find(info->store, owner);
	FluxNodeData const *root = nd && nd->uia_popup ? flux_store_find(info->store, nd->uia_popup) : NULL;
	return root && root->popup && flux_popup_visible(root->popup) ? nd->uia_popup : XENT_NODE_INVALID;
}

static XentNodeId tree_parent(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId owner = uia_owner_of(info, node);
	return owner ? owner : xent_node_parent(info->ctx, node);
}

static XentNodeId tree_first(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId c = xent_node_first(info->ctx, node);
	return c ? c : uia_popup_of(info, node);
}

static XentNodeId tree_last(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId c = uia_popup_of(info, node);
	return c ? c : xent_node_last(info->ctx, node);
}

static XentNodeId tree_next(FluxUiaContext const *info, XentNodeId node) {
	if (uia_owner_of(info, node)) return XENT_NODE_INVALID;
	XentNodeId s = xent_node_next(info->ctx, node);
	if (s) return s;
	XentNodeId p = xent_node_parent(info->ctx, node);
	return p ? uia_popup_of(info, p) : XENT_NODE_INVALID;
}

static XentNodeId tree_prev(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId owner = uia_owner_of(info, node);
	return owner ? xent_node_last(info->ctx, owner) : xent_node_prev(info->ctx, node);
}

static bool uia_hidden(FluxUiaContext const *info, XentNodeId node) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	return h && h->role == FLUX_UIA_HIDDEN;
}

bool flux_uia_exposed(FluxUiaContext const *info, XentNodeId node) {
	if (node == info->root) return true;
	return !xent_collapsed(info->ctx, node) && !uia_hidden(info, node);
}

XentNodeId flux_uia_parent(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId p = tree_parent(info, node);
	while (p != XENT_NODE_INVALID && p != info->root && !flux_uia_exposed(info, p)) p = tree_parent(info, p);
	return p;
}

XentNodeId flux_uia_visible(FluxUiaContext const *info, XentNodeId node) {
	return node == XENT_NODE_INVALID || flux_uia_exposed(info, node) ? node : flux_uia_parent(info, node);
}

static XentNodeId uia_edge(FluxUiaContext const *info, XentNodeId node, UiaStep start, UiaStep step) {
	for (XentNodeId c = start(info, node); c != XENT_NODE_INVALID; c = step(info, c)) {
		if (xent_collapsed(info->ctx, c)) continue;
		if (!uia_hidden(info, c)) return c;
		XentNodeId inner = uia_edge(info, c, start, step);
		if (inner != XENT_NODE_INVALID) return inner;
	}
	return XENT_NODE_INVALID;
}

XentNodeId flux_uia_first(FluxUiaContext const *info, XentNodeId node) {
	return uia_edge(info, node, tree_first, tree_next);
}

XentNodeId flux_uia_last(FluxUiaContext const *info, XentNodeId node) {
	return uia_edge(info, node, tree_last, tree_prev);
}

static XentNodeId uia_sibling(FluxUiaContext const *info, XentNodeId node, UiaStep step, UiaStep inner_start) {
	for (XentNodeId s = step(info, node); s != XENT_NODE_INVALID; s = step(info, s)) {
		if (xent_collapsed(info->ctx, s)) continue;
		if (!uia_hidden(info, s)) return s;
		XentNodeId inner = uia_edge(info, s, inner_start, step);
		if (inner != XENT_NODE_INVALID) return inner;
	}
	XentNodeId p = tree_parent(info, node);
	if (p == XENT_NODE_INVALID || p == info->root || flux_uia_exposed(info, p)) return XENT_NODE_INVALID;
	return uia_sibling(info, p, step, inner_start);
}

XentNodeId flux_uia_next(FluxUiaContext const *info, XentNodeId node) {
	return uia_sibling(info, node, tree_next, tree_first);
}

XentNodeId flux_uia_prev(FluxUiaContext const *info, XentNodeId node) {
	return uia_sibling(info, node, tree_prev, tree_last);
}
