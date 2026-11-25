#include "a11y/flux_uia_object.h"
#include "a11y/flux_uia_app.h"

#include <string.h>

#define REFS_MAX 16

static XentNodeId refs_find(FluxUiaContext const *info, XentNodeId node, char const *id, size_t len) {
	char const *mine = flux_uia_apptext(info->store, node, FLUX_A11Y_ID);
	if (mine && strlen(mine) == len && strncmp(mine, id, len) == 0) return node;
	for (XentNodeId c = xent_node_first(info->ctx, node); c != XENT_NODE_INVALID; c = xent_node_next(info->ctx, c)) {
		XentNodeId hit = refs_find(info, c, id, len);
		if (hit != XENT_NODE_INVALID) return hit;
	}
	return XENT_NODE_INVALID;
}

int flux_uia_refs(FluxUiaContext const *info, XentNodeId node, int slot, XentNodeId *out, int cap) {
	char const *list = flux_uia_apptext(info->store, node, slot);
	int         n    = 0;
	while (list && *list && n < cap) {
		char const *end = strchr(list, ';');
		size_t      len = end ? ( size_t ) (end - list) : strlen(list);
		XentNodeId  hit = len ? refs_find(info, info->root, list, len) : XENT_NODE_INVALID;
		if (hit != XENT_NODE_INVALID) out [n++] = hit;
		list += len + (end ? 1u : 0u);
	}
	return n;
}

void flux_uia_refs_variant(FluxUiaContext const *info, XentNodeId const *nodes, int count, VARIANT *out) {
	SAFEARRAY *sa = count > 0 ? SafeArrayCreateVector(VT_UNKNOWN, 0, ( ULONG ) count) : NULL;
	for (LONG i = 0; sa && i < count; i++) {
		FluxUia *p = flux_uia_alloc(info, nodes [i]);
		if (!p) continue;
		SafeArrayPutElement(sa, &i, ( IUnknown * ) &p->lpVtblSimple);
		flux_uia_release(p);
	}
	if (!sa) return;
	out->vt     = VT_UNKNOWN | VT_ARRAY;
	out->parray = sa;
}

void flux_uia_refs_prop(FluxUiaContext const *info, XentNodeId node, int slot, XentNodeId extra, VARIANT *out) {
	XentNodeId nodes [REFS_MAX];
	int        n = 0;
	if (extra != XENT_NODE_INVALID) nodes [n++] = extra;
	n += flux_uia_refs(info, node, slot, nodes + n, REFS_MAX - n);
	flux_uia_refs_variant(info, nodes, n, out);
}
