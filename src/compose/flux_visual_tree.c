#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_visual_tree.h"
#include "compose/flux_interop.h"

#include <cwinrt/cast.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct VisNode {
	WUC_Container *container; /**< This node's container visual (we hold one ref). */
	WUC_Visual    *visual;    /**< IVisual facet of container (offset/size + collection ops). */
	XentNodeId     parent;    /**< Parent node id, for detach on removal. */
	float          abs_x;     /**< Absolute layout x, so children can offset relatively. */
	float          abs_y;
	uint32_t       seen;      /**< Last generation this node was reconciled. */
	bool           in_use;
} VisNode;

struct FluxVisualTree {
	WUC_Comp      *c;     /**< Compositor (not owned). */
	WUC_Container *root;  /**< Parent container the mirror hangs under (we hold a ref). */
	VisNode       *nodes; /**< Indexed by XentNodeId. */
	uint32_t       cap;
	uint32_t       generation;
};

FluxVisualTree *flux_visual_tree_create(FluxCompositor *c, WUC_Visual *root_parent) {
	if (!c || !root_parent) return NULL;
	WUC_Comp *comp = flux_compositor_comp(c);
	if (!comp) return NULL;

	FluxVisualTree *vt = ( FluxVisualTree * ) calloc(1, sizeof(*vt));
	if (!vt) return NULL;
	vt->c = comp;

	/* SpriteVisual and ContainerVisual both implement IContainerVisual. */
	if (FAILED(cwinrt_query(root_parent, &CWINRT_IID_WUC_IContainerVisual, ( void ** ) &vt->root))) {
		free(vt);
		return NULL;
	}
	return vt;
}

static bool tree_reserve(FluxVisualTree *vt, XentNodeId node) {
	if (node < vt->cap) return true;
	uint32_t new_cap = vt->cap ? vt->cap * 2 : 64;
	while (new_cap <= node) new_cap *= 2;
	VisNode *n = ( VisNode * ) realloc(vt->nodes, ( size_t ) new_cap * sizeof(*n));
	if (!n) return false;
	memset(n + vt->cap, 0, ( size_t ) (new_cap - vt->cap) * sizeof(*n));
	vt->nodes = n;
	vt->cap   = new_cap;
	return true;
}

static void tree_insert_child(WUC_Container *parent, WUC_Visual *child) {
	WUC_VisualCollection *children = NULL;
	if (FAILED(wuc_container_get__children(parent, &children))) return;
	( void ) wuc_visual_collection_insert_at_top(children, child);
	(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
}

static void tree_remove_child(WUC_Container *parent, WUC_Visual *child) {
	WUC_VisualCollection *children = NULL;
	if (FAILED(wuc_container_get__children(parent, &children))) return;
	( void ) wuc_visual_collection_remove(children, child);
	(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
}

/* Ensure a container exists for `node` under `parent_node`, then position it
 * relative to the parent. Parent is always reconciled before its children, so
 * its entry is valid here. Returns the container, or NULL on failure. */
static WUC_Container *
tree_sync_node(FluxVisualTree *vt, XentNodeId node, XentNodeId parent_node, XentRect const *rect) {
	if (!tree_reserve(vt, node)) return NULL;

	WUC_Container *parent_container = parent_node == XENT_NODE_INVALID ? vt->root : vt->nodes [parent_node].container;
	if (!parent_container) return NULL;

	VisNode *vn = &vt->nodes [node];
	if (!vn->in_use) {
		if (FAILED(wuc_comp_create_container_visual(vt->c, &vn->container))) return NULL;
		vn->visual = flux_compose_as_visual(vn->container);
		if (!vn->visual) {
			(( IUnknown * ) vn->container)->lpVtbl->Release(( IUnknown * ) vn->container);
			vn->container = NULL;
			return NULL;
		}
		vn->in_use = true;
		tree_insert_child(parent_container, vn->visual);
	}
	vn->parent       = parent_node;

	float        pax = parent_node == XENT_NODE_INVALID ? 0.0f : vt->nodes [parent_node].abs_x;
	float        pay = parent_node == XENT_NODE_INVALID ? 0.0f : vt->nodes [parent_node].abs_y;
	WFN_Vector_3 off = {rect->x - pax, rect->y - pay, 0.0f};
	( void ) wuc_visual_put__offset(vn->visual, off);
	WFN_Vector_2 size = {rect->w, rect->h};
	( void ) wuc_visual_put__size(vn->visual, size);

	vn->abs_x = rect->x;
	vn->abs_y = rect->y;
	vn->seen  = vt->generation;
	return vn->container;
}

static void tree_sweep(FluxVisualTree *vt) {
	for (uint32_t id = 0; id < vt->cap; id++) {
		VisNode *vn = &vt->nodes [id];
		if (!vn->in_use || vn->seen == vt->generation) continue;

		/* Detach from the parent only if the parent survived this pass; if the
		 * parent is also gone, releasing it detaches the whole subtree at once. */
		bool parent_alive
		  = vn->parent == XENT_NODE_INVALID
		 || (vn->parent < vt->cap && vt->nodes [vn->parent].in_use && vt->nodes [vn->parent].seen == vt->generation);
		WUC_Container *pc
		  = vn->parent == XENT_NODE_INVALID ? vt->root : (parent_alive ? vt->nodes [vn->parent].container : NULL);
		if (pc) tree_remove_child(pc, vn->visual);

		(( IUnknown * ) vn->visual)->lpVtbl->Release(( IUnknown * ) vn->visual);
		(( IUnknown * ) vn->container)->lpVtbl->Release(( IUnknown * ) vn->container);
		vn->visual    = NULL;
		vn->container = NULL;
		vn->in_use    = false;
		vn->parent    = XENT_NODE_INVALID;
	}
}

typedef struct ReconcileFrame {
	XentNodeId node;
	XentNodeId parent;
} ReconcileFrame;

void flux_visual_tree_reconcile(FluxVisualTree *vt, XentContext *ctx, XentNodeId root) {
	if (!vt || !ctx || root == XENT_NODE_INVALID) return;
	vt->generation++;

	uint32_t        cap   = 64;
	uint32_t        top   = 0;
	ReconcileFrame *stack = ( ReconcileFrame * ) malloc(sizeof(*stack) * cap);
	if (!stack) return;

	stack [top++] = (ReconcileFrame) {root, XENT_NODE_INVALID};

	while (top > 0) {
		ReconcileFrame f    = stack [--top];
		XentRect       rect = {0};
		xent_get_layout_rect(ctx, f.node, &rect);

		if (!tree_sync_node(vt, f.node, f.parent, &rect)) continue;

		for (XentNodeId child = xent_get_first_child(ctx, f.node); child != XENT_NODE_INVALID;
		  child               = xent_get_next_sibling(ctx, child))
		{
			if (top == cap) {
				uint32_t        ncap = cap * 2;
				ReconcileFrame *ns   = ( ReconcileFrame * ) realloc(stack, sizeof(*stack) * ncap);
				if (!ns) goto done;
				stack = ns;
				cap   = ncap;
			}
			stack [top++] = (ReconcileFrame) {child, f.node};
		}
	}

done:
	free(stack);
	tree_sweep(vt);
}

WUC_Container *flux_visual_tree_node_visual(FluxVisualTree *vt, XentNodeId node) {
	if (!vt || node >= vt->cap || !vt->nodes [node].in_use) return NULL;
	return vt->nodes [node].container;
}

void flux_visual_tree_destroy(FluxVisualTree *vt) {
	if (!vt) return;
	for (uint32_t id = 0; id < vt->cap; id++) {
		if (vt->nodes [id].visual)
			(( IUnknown * ) vt->nodes [id].visual)->lpVtbl->Release(( IUnknown * ) vt->nodes [id].visual);
		if (vt->nodes [id].in_use && vt->nodes [id].container)
			(( IUnknown * ) vt->nodes [id].container)->lpVtbl->Release(( IUnknown * ) vt->nodes [id].container);
	}
	if (vt->root) {
		WUC_VisualCollection *children = NULL;
		if (SUCCEEDED(wuc_container_get__children(vt->root, &children))) {
			( void ) wuc_visual_collection_remove_all(children);
			(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
		}
		(( IUnknown * ) vt->root)->lpVtbl->Release(( IUnknown * ) vt->root);
	}
	free(vt->nodes);
	free(vt);
}
