#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_visual_tree.h"
#include "store/flux_node_data.h"
#include "compose/flux_interop.h"

#include "controls/data/flux_scroll_data.h"
#include "fluxent/flux_control_type.h"

#include <cwinrt/cast.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct VisNode {
	XentNodeId     owner;
	WUC_Container *container;
	WUC_Visual    *visual;
	WUC_Container *holder;
	WUC_Visual    *holder_visual;
	XentNodeId     parent;
	float          abs_x;
	float          abs_y;
	uint32_t       seen;
	bool           in_use;
	bool           clipped;
	bool           tracker_bound;
} VisNode;

struct FluxVisualTree {
	WUC_Comp      *c;
	WUC_Container *root;
	VisNode       *nodes;
	uint32_t       cap;
	uint32_t       generation;
	float          scale;
	XentCtx       *ctx;
	FluxStore     *store;
};

FluxVisualTree *flux_visual_tree_create(FluxCompositor *c, WUC_Visual *root_parent) {
	if (!c || !root_parent) return NULL;
	WUC_Comp *comp = flux_compositor_comp(c);
	if (!comp) return NULL;

	FluxVisualTree *vt = ( FluxVisualTree * ) calloc(1, sizeof(*vt));
	if (!vt) return NULL;
	vt->c = comp;

	if (FAILED(cwinrt_query(root_parent, &CWINRT_IID_WUC_IContainerVisual, ( void ** ) &vt->root))) {
		free(vt);
		return NULL;
	}
	return vt;
}

static bool tree_reserve(FluxVisualTree *vt, XentNodeId node) {
	uint32_t idx = xent_node_index(node);
	if (idx < vt->cap) return true;
	uint32_t new_cap = vt->cap ? vt->cap * 2 : 64;
	while (new_cap <= idx) {
		if (new_cap > UINT32_MAX / 2u) return false;
		new_cap *= 2;
	}
	VisNode *n = ( VisNode * ) realloc(vt->nodes, ( size_t ) new_cap * sizeof(*n));
	if (!n) return false;
	memset(n + vt->cap, 0, ( size_t ) (new_cap - vt->cap) * sizeof(*n));
	vt->nodes = n;
	vt->cap   = new_cap;
	return true;
}

static VisNode *tree_owned(FluxVisualTree *vt, XentNodeId node) {
	uint32_t idx = xent_node_index(node);
	if (!vt || idx >= vt->cap) return NULL;
	VisNode *vn = &vt->nodes [idx];
	return (vn->in_use && vn->owner == node) ? vn : NULL;
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

static void tree_evict(FluxVisualTree *vt, VisNode *vn) {
	uint32_t parent_idx = xent_node_index(vn->parent);
	bool     parent_alive
	  = vn->parent == XENT_NODE_INVALID
	 || (parent_idx < vt->cap && vt->nodes [parent_idx].in_use && vt->nodes [parent_idx].owner == vn->parent);
	WUC_Container *pc = NULL;
	if (parent_alive) {
		if (vn->parent == XENT_NODE_INVALID) pc = vt->root;
		else {
			VisNode *parent = &vt->nodes [parent_idx];
			pc              = parent->holder ? parent->holder : parent->container;
		}
	}
	if (pc && vn->visual) tree_remove_child(pc, vn->visual);
	if (vn->holder) (( IUnknown * ) vn->holder)->lpVtbl->Release(( IUnknown * ) vn->holder);
	if (vn->holder_visual) (( IUnknown * ) vn->holder_visual)->lpVtbl->Release(( IUnknown * ) vn->holder_visual);
	if (vn->visual) (( IUnknown * ) vn->visual)->lpVtbl->Release(( IUnknown * ) vn->visual);
	if (vn->container) (( IUnknown * ) vn->container)->lpVtbl->Release(( IUnknown * ) vn->container);
	vn->holder        = NULL;
	vn->holder_visual = NULL;
	vn->visual        = NULL;
	vn->container     = NULL;
	vn->in_use        = false;
	vn->clipped       = false;
	vn->tracker_bound = false;
	vn->parent        = XENT_NODE_INVALID;
	vn->owner         = XENT_NODE_INVALID;
}

static void tree_node_scroll(FluxVisualTree const *vt, XentNodeId node, float *sx, float *sy) {
	*sx = 0.0f;
	*sy = 0.0f;
	if (!vt->store || flux_node_type(vt->store, node) != FLUX_CONTROL_SCROLL) return;
	FluxNodeData   *nd = flux_store_find(vt->store, node);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!sd) return;
	*sx = flux_scroll_off_x(sd);
	*sy = flux_scroll_off_y(sd);
}

static void tree_clip_to_bounds(FluxVisualTree *vt, VisNode *vn) {
	if (vn->clipped) return;
	WUC_InsetClip *inset = NULL;
	if (FAILED(wuc_comp_create_inset_clip(vt->c, &inset))) return;
	WUC_CompositionClip *clip = NULL;
	if (SUCCEEDED(cwinrt_query(inset, &CWINRT_IID_WUC_ICompositionClip, ( void ** ) &clip))) {
		( void ) wuc_visual_put__clip(vn->visual, clip);
		(( IUnknown * ) clip)->lpVtbl->Release(( IUnknown * ) clip);
		vn->clipped = true;
	}
	(( IUnknown * ) inset)->lpVtbl->Release(( IUnknown * ) inset);
}

static WUC_Container *tree_child_parent(FluxVisualTree *vt, XentNodeId parent_node) {
	if (parent_node == XENT_NODE_INVALID) return vt->root;
	VisNode *vn = tree_owned(vt, parent_node);
	if (!vn) return NULL;
	return vn->holder ? vn->holder : vn->container;
}

static void tree_make_holder(FluxVisualTree *vt, VisNode *vn) {
	if (vn->holder) return;
	if (FAILED(wuc_comp_create_container_visual(vt->c, &vn->holder))) return;
	vn->holder_visual = flux_compose_as_visual(vn->holder);
	if (!vn->holder_visual) {
		(( IUnknown * ) vn->holder)->lpVtbl->Release(( IUnknown * ) vn->holder);
		vn->holder = NULL;
		return;
	}
	tree_insert_child(vn->container, vn->holder_visual);
}

static bool tree_ensure_container(FluxVisualTree *vt, VisNode *vn, XentNodeId node, WUC_Container *parent_container) {
	if (vn->in_use && vn->owner == node) return true;
	if (vn->in_use) tree_evict(vt, vn);
	if (FAILED(wuc_comp_create_container_visual(vt->c, &vn->container))) return false;
	vn->visual = flux_compose_as_visual(vn->container);
	if (!vn->visual) {
		(( IUnknown * ) vn->container)->lpVtbl->Release(( IUnknown * ) vn->container);
		vn->container = NULL;
		return false;
	}
	vn->owner  = node;
	vn->in_use = true;
	tree_insert_child(parent_container, vn->visual);
	return true;
}

static WUC_Container *
tree_sync_node(FluxVisualTree *vt, XentNodeId node, XentNodeId parent_node, XentRect const *rect) {
	if (!tree_reserve(vt, node)) return NULL;

	WUC_Container *parent_container = tree_child_parent(vt, parent_node);
	if (!parent_container) return NULL;

	VisNode *vn = &vt->nodes [xent_node_index(node)];
	if (!tree_ensure_container(vt, vn, node, parent_container)) return NULL;
	vn->parent             = parent_node;

	VisNode     *parent_vn = parent_node == XENT_NODE_INVALID ? NULL : tree_owned(vt, parent_node);
	float        pax       = parent_vn ? parent_vn->abs_x : 0.0f;
	float        pay       = parent_vn ? parent_vn->abs_y : 0.0f;
	WFN_Vector_3 off       = {(rect->x - pax) * vt->scale, (rect->y - pay) * vt->scale, 0.0f};
	( void ) wuc_visual_put__offset(vn->visual, off);
	WFN_Vector_2 size = {rect->w * vt->scale, rect->h * vt->scale};
	( void ) wuc_visual_put__size(vn->visual, size);

	if (flux_node_type(vt->store, node) == FLUX_CONTROL_SCROLL) {
		tree_clip_to_bounds(vt, vn);
		tree_make_holder(vt, vn);
	}
	float sx, sy;
	tree_node_scroll(vt, node, &sx, &sy);
	if (vn->tracker_bound) sx = sy = 0.0f;

	vn->abs_x = rect->x + sx;
	vn->abs_y = rect->y + sy;
	vn->seen  = vt->generation;
	return vn->container;
}

static void tree_sweep(FluxVisualTree *vt) {
	for (uint32_t id = 0; id < vt->cap; id++) {
		VisNode *vn = &vt->nodes [id];
		if (!vn->in_use || vn->seen == vt->generation) continue;

		VisNode *parent_vn    = vn->parent == XENT_NODE_INVALID ? NULL : tree_owned(vt, vn->parent);
		bool     parent_alive = vn->parent == XENT_NODE_INVALID || (parent_vn && parent_vn->seen == vt->generation);
		WUC_Container *pc     = parent_alive ? tree_child_parent(vt, vn->parent) : NULL;
		if (pc) tree_remove_child(pc, vn->visual);

		if (vn->holder) (( IUnknown * ) vn->holder)->lpVtbl->Release(( IUnknown * ) vn->holder);
		if (vn->holder_visual) (( IUnknown * ) vn->holder_visual)->lpVtbl->Release(( IUnknown * ) vn->holder_visual);
		(( IUnknown * ) vn->visual)->lpVtbl->Release(( IUnknown * ) vn->visual);
		(( IUnknown * ) vn->container)->lpVtbl->Release(( IUnknown * ) vn->container);
		vn->holder        = NULL;
		vn->holder_visual = NULL;
		vn->visual        = NULL;
		vn->container     = NULL;
		vn->in_use        = false;
		vn->clipped       = false;
		vn->tracker_bound = false;
		vn->parent        = XENT_NODE_INVALID;
		vn->owner         = XENT_NODE_INVALID;
	}
}

typedef struct ReconcileFrame {
	XentNodeId node;
	XentNodeId parent;
} ReconcileFrame;

static bool reconcile_stack_push(ReconcileFrame **stack, uint32_t *top, uint32_t *cap, ReconcileFrame f) {
	if (*top == *cap) {
		uint32_t        ncap = *cap * 2;
		ReconcileFrame *ns   = ( ReconcileFrame * ) realloc(*stack, sizeof(**stack) * ncap);
		if (!ns) return false;
		*stack = ns;
		*cap   = ncap;
	}
	(*stack) [(*top)++] = f;
	return true;
}

static bool
reconcile_push_children(ReconcileFrame **stack, uint32_t *top, uint32_t *cap, XentCtx *ctx, XentNodeId parent) {
	for (XentNodeId child = xent_node_first(ctx, parent); child != XENT_NODE_INVALID;
	  child               = xent_node_next(ctx, child))
		if (!reconcile_stack_push(stack, top, cap, (ReconcileFrame) {child, parent})) return false;
	return true;
}

void flux_visual_tree_reconcile(FluxVisualTree *vt, XentCtx *ctx, FluxStore *store, XentNodeId root, float scale) {
	if (!vt || !ctx || root == XENT_NODE_INVALID) return;
	vt->ctx   = ctx;
	vt->store = store;
	vt->scale = scale > 0.0f ? scale : 1.0f;
	vt->generation++;

	uint32_t        cap   = 64;
	uint32_t        top   = 0;
	ReconcileFrame *stack = ( ReconcileFrame * ) malloc(sizeof(*stack) * cap);
	if (!stack) return;

	stack [top++] = (ReconcileFrame) {root, XENT_NODE_INVALID};

	while (top > 0) {
		ReconcileFrame f    = stack [--top];
		XentRect       rect = {0};
		xent_layout_rect(ctx, f.node, &rect);

		if (!tree_sync_node(vt, f.node, f.parent, &rect)) continue;
		if (!reconcile_push_children(&stack, &top, &cap, ctx, f.node)) goto done;
	}

done:
	free(stack);
	tree_sweep(vt);
}

WUC_Container *flux_vtree_visual(FluxVisualTree *vt, XentNodeId node) {
	VisNode *vn = tree_owned(vt, node);
	return vn ? vn->container : NULL;
}

WUC_Visual *flux_vtree_scrollholder(FluxVisualTree *vt, XentNodeId node) {
	VisNode *vn = tree_owned(vt, node);
	return vn ? vn->holder_visual : NULL;
}

void flux_vtree_bindtracker(FluxVisualTree *vt, XentNodeId node, bool bound) {
	VisNode *vn = tree_owned(vt, node);
	if (vn) vn->tracker_bound = bound;
}

static void tree_release_node(VisNode const *n) {
	if (n->holder_visual) (( IUnknown * ) n->holder_visual)->lpVtbl->Release(( IUnknown * ) n->holder_visual);
	if (n->holder) (( IUnknown * ) n->holder)->lpVtbl->Release(( IUnknown * ) n->holder);
	if (n->visual) (( IUnknown * ) n->visual)->lpVtbl->Release(( IUnknown * ) n->visual);
	if (n->in_use && n->container) (( IUnknown * ) n->container)->lpVtbl->Release(( IUnknown * ) n->container);
}

static void tree_release_root(WUC_Container *root) {
	WUC_VisualCollection *children = NULL;
	if (SUCCEEDED(wuc_container_get__children(root, &children))) {
		( void ) wuc_visual_collection_remove_all(children);
		(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
	}
	(( IUnknown * ) root)->lpVtbl->Release(( IUnknown * ) root);
}

void flux_visual_tree_destroy(FluxVisualTree *vt) {
	if (!vt) return;
	for (uint32_t id = 0; id < vt->cap; id++) tree_release_node(&vt->nodes [id]);
	if (vt->root) tree_release_root(vt->root);
	free(vt->nodes);
	free(vt);
}
