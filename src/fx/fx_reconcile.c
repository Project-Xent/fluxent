/**
 * @file fx_reconcile.c
 * @brief FX reconciler: single-pass keyed diff of the new element tree
 * against the previous frame, applied to retained controls.
 *
 * Matching per parent: keyed children match an unclaimed old child with the
 * same key and type; unkeyed children match the old unkeyed child at the
 * same unkeyed ordinal when types agree. Matched nodes are updated through
 * property setters (retained state survives); everything else mounts fresh
 * or unmounts via flux_subtree_destroy. Child order is restored with
 * re-appends only when the match order changed.
 *
 * Per-type property application, control creation, and mount setup live in
 * fx_dispatch_create.c / fx_dispatch_props.c and are reached through the
 * declarations in fx_internal.h.
 */
#include "fx_internal.h"

#include <stdlib.h>
#include <string.h>

static FxNode *fx_node_create(XentNodeId id, FxBinding *binding) {
	FxNode *n = ( FxNode * ) calloc(1, sizeof(*n));
	if (!n) return NULL;
	n->node    = id;
	n->host    = id;
	n->binding = binding;
	return n;
}

static void fx_node_free_tree(FxNode *n) {
	if (!n) return;
	for (int i = 0; i < n->child_count; i++) fx_node_free_tree(n->children [i]);
	if (n->menu) flux_menu_flyout_destroy(n->menu);
	free(n->menu_bindings);
	free(n->tab_hosts);
	free(n->children);
	free(n->binding);
	free(n);
}

static bool fx_node_reserve(FxNode *n, int count) {
	if (count <= n->child_cap) return true;
	int cap = n->child_cap ? n->child_cap * 2 : 8;
	if (cap < count) cap = count;
	FxNode **items = ( FxNode ** ) realloc(n->children, sizeof(FxNode *) * ( size_t ) cap);
	if (!items) return false;
	n->children  = items;
	n->child_cap = cap;
	return true;
}

static FxNode *fx_mount(FxRuntime *rt, XentNodeId parent, FluxEl const *el) {
	FxBinding *b = NULL;
	if (fx_el_interactive(el)) {
		b = ( FxBinding * ) calloc(1, sizeof(*b));
		if (!b) return NULL;
		b->rt = rt;
	}

	XentNodeId id = fx_create_control(rt, parent, el, b);
	if (id == XENT_NODE_INVALID) {
		free(b);
		return NULL;
	}

	FxNode *n = fx_node_create(id, b);
	if (!n) {
		flux_subtree_destroy(rt->store, id);
		free(b);
		return NULL;
	}

	fx_mount_setup(rt, n, id, el);

	if (el->child_count > 0 && fx_node_reserve(n, el->child_count)) {
		for (int i = 0; i < el->child_count; i++) n->children [i] = fx_mount(rt, n->host, el->children [i]);
		n->child_count = el->child_count;
	}
	return n;
}

static void fx_unmount(FxRuntime *rt, FxNode *n) {
	if (!n) return;
	if (flux_get_control_type(rt->ctx, n->node) == FLUX_CONTROL_CONTENT_DIALOG)
		flux_content_dialog_hide(rt->store, n->node);
	flux_subtree_destroy(rt->store, n->node);
	fx_node_free_tree(n);
}

static bool fx_same_identity(FluxEl const *a, FluxEl const *b) {
	if (a->type != b->type) return false;
	if (!!a->key != !!b->key) return false;
	return !a->key || strcmp(a->key, b->key) == 0;
}

static void fx_diff(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el);

static int fx_match_keyed(FluxEl const *prev, bool const *claimed, FluxEl const *want) {
	for (int i = 0; i < prev->child_count; i++)
		if (!claimed [i] && fx_same_identity(prev->children [i], want)) return i;
	return -1;
}

static int fx_match_child(FluxEl const *prev, bool const *claimed, FluxEl const *want, int *unkeyed_cursor) {
	if (want->key) return fx_match_keyed(prev, claimed, want);
	int seen = 0;
	for (int i = 0; i < prev->child_count; i++) {
		if (prev->children [i]->key) continue;
		if (seen++ < *unkeyed_cursor) continue;
		(*unkeyed_cursor)++;
		return !claimed [i] && fx_same_identity(prev->children [i], want) ? i : -1;
	}
	(*unkeyed_cursor)++;
	return -1;
}

static void fx_diff_match(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el, FxNode **out, bool *claimed, int *last_old, bool *reorder) {
	int unkeyed_cursor = 0;
	for (int i = 0; i < el->child_count; i++) {
		FluxEl const *want = el->children [i];
		int           from = fx_match_child(prev, claimed, want, &unkeyed_cursor);
		if (from >= 0 && from < n->child_count) {
			claimed [from] = true;
			out [i]        = n->children [from];
			fx_diff(rt, out [i], prev->children [from], want);
			*reorder = *reorder || from < *last_old;
			*last_old = from;
		} else {
			out [i]  = fx_mount(rt, n->host, want);
			*reorder = true;
		}
	}
}

static void fx_diff_reorder(FxRuntime *rt, FxNode *n, FluxEl const *el) {
	for (int i = 0; i < n->child_count; i++)
		if (n->children [i] && el->children [i]->type != FLUX_CONTROL_CONTENT_DIALOG)
			xent_append_child(rt->ctx, n->host, n->children [i]->node);
}

static void fx_diff_children(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	int   old_count = prev->child_count;
	bool *claimed   = old_count ? ( bool * ) calloc(( size_t ) old_count, sizeof(bool)) : NULL;
	if (old_count && !claimed) return;

	FxNode **next_nodes = el->child_count ? ( FxNode ** ) calloc(( size_t ) el->child_count, sizeof(FxNode *)) : NULL;
	if (el->child_count && !next_nodes) {
		free(claimed);
		return;
	}

	int  last_old = -1;
	bool reorder  = false;
	fx_diff_match(rt, n, prev, el, next_nodes, claimed, &last_old, &reorder);

	for (int i = 0; i < old_count; i++)
		if (!claimed [i] && i < n->child_count) fx_unmount(rt, n->children [i]);
	free(claimed);

	free(n->children);
	n->children    = next_nodes;
	n->child_count = el->child_count;
	n->child_cap   = el->child_count;

	if (reorder) fx_diff_reorder(rt, n, el);
}

static void fx_diff(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (!n) return;
	fx_apply_props(rt, n, prev, el);
	fx_diff_children(rt, n, prev, el);
}

static void fx_mount_root(FxRuntime *rt, FluxEl *next) {
	rt->root = fx_mount(rt, rt->host, next);
	if (!rt->root) return;
	xent_set_absolute_position(rt->ctx, rt->root->node, (XentPoint) {0.0f, 0.0f});
	if (next->type != FLUX_CONTROL_NAV_VIEW)
		xent_set_size_percent(rt->ctx, rt->root->node, (XentSize) {1.0f, 1.0f});
}

void fx_reconcile(FxRuntime *rt, FluxEl *next) {
	if (!next) return;
	if (!rt->root || !rt->prev_root || !fx_same_identity(rt->prev_root, next)) {
		if (rt->root) fx_unmount(rt, rt->root);
		fx_mount_root(rt, next);
		rt->prev_root = next;
		return;
	}
	fx_diff(rt, rt->root, rt->prev_root, next);
	rt->prev_root = next;
}

void fx_unmount_all(FxRuntime *rt) {
	if (!rt || !rt->root) return;
	fx_unmount(rt, rt->root);
	rt->root      = NULL;
	rt->prev_root = NULL;
}
