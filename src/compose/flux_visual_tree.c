#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_visual_tree.h"
#include "compose/flux_interop.h"

#include "fluxent/controls/flux_scroll_data.h"
#include "fluxent/flux_control_type.h"

#include <cwinrt/cast.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct VisNode {
	WUC_Container *container;     /**< This node's container visual (we hold one ref). */
	WUC_Visual    *visual;        /**< IVisual facet of container (offset/size + collection ops). */
	WUC_Container *holder;        /**< Scroll node only: content sub-visual the children attach to,
	                               *   so an InteractionTracker can move all content as one (NULL otherwise). */
	WUC_Visual    *holder_visual; /**< IVisual facet of holder (offset binding target). */
	XentNodeId     parent;        /**< Parent node id, for detach on removal. */
	float          abs_x;         /**< Absolute layout x, so children can offset relatively. */
	float          abs_y;
	uint32_t       seen;          /**< Last generation this node was reconciled. */
	bool           in_use;
	bool           clipped;       /**< Scroll node: an inset clip is installed on the container. */
	bool           tracker_bound; /**< Scroll node: holder Offset is driven by an InteractionTracker. */
} VisNode;

struct FluxVisualTree {
	WUC_Comp      *c;     /**< Compositor (not owned). */
	WUC_Container *root;  /**< Parent container the mirror hangs under (we hold a ref). */
	VisNode       *nodes; /**< Indexed by XentNodeId. */
	uint32_t       cap;
	uint32_t       generation;
	float          scale; /**< DIP-to-pixel factor for this pass (dpi/96). */
	XentContext   *ctx;   /**< Layout context for this pass (control-type lookups). */
	FluxNodeStore *store; /**< Node store for this pass (scroll offsets). */
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

/* A scroll node's current offset (DIPs), or {0,0} for any other node. The
 * compositor mirror has no command list, so scrolling is expressed structurally:
 * the offset is folded into the base that children inherit (below), and an inset
 * clip hides the overflow -- the retained-tree equivalent of the classic path's
 * per-frame clip + translate transform. */
static void tree_node_scroll(FluxVisualTree const *vt, XentNodeId node, float *sx, float *sy) {
	*sx = 0.0f;
	*sy = 0.0f;
	if (!vt->store || flux_get_control_type(vt->ctx, node) != FLUX_CONTROL_SCROLL) return;
	FluxNodeData   *nd = flux_node_store_get(vt->store, node);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!sd) return;
	*sx = sd->scroll_x;
	*sy = sd->scroll_y;
}

/* Install a zero-inset clip so the container crops its children to its own size
 * (the scroll viewport). Idempotent: the clip tracks the size set each frame, so
 * it is created once per scroll node. */
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

/* Children attach to a scroll node's content holder (so the tracker moves them as
 * one); to the plain container otherwise. */
static WUC_Container *tree_child_parent(FluxVisualTree *vt, XentNodeId parent_node) {
	if (parent_node == XENT_NODE_INVALID) return vt->root;
	VisNode *vn = &vt->nodes [parent_node];
	return vn->holder ? vn->holder : vn->container;
}

/* Create the content holder under a scroll node's container (once). The holder
 * carries all child content so its Offset can be driven by an InteractionTracker;
 * the container stays put (clip + scrollbar) while the holder scrolls. */
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

/* Ensure a container exists for `node` under `parent_node`, then position it
 * relative to the parent. Parent is always reconciled before its children, so
 * its entry is valid here. Returns the container, or NULL on failure. */
static bool tree_ensure_container(FluxVisualTree *vt, VisNode *vn, WUC_Container *parent_container) {
	if (vn->in_use) return true;
	if (FAILED(wuc_comp_create_container_visual(vt->c, &vn->container))) return false;
	vn->visual = flux_compose_as_visual(vn->container);
	if (!vn->visual) {
		(( IUnknown * ) vn->container)->lpVtbl->Release(( IUnknown * ) vn->container);
		vn->container = NULL;
		return false;
	}
	vn->in_use = true;
	tree_insert_child(parent_container, vn->visual);
	return true;
}

static WUC_Container *
tree_sync_node(FluxVisualTree *vt, XentNodeId node, XentNodeId parent_node, XentRect const *rect) {
	if (!tree_reserve(vt, node)) return NULL;

	WUC_Container *parent_container = tree_child_parent(vt, parent_node);
	if (!parent_container) return NULL;

	VisNode *vn = &vt->nodes [node];
	if (!tree_ensure_container(vt, vn, parent_container)) return NULL;
	vn->parent       = parent_node;

	/* abs_x/abs_y stay in DIPs so child-relative math is unaffected; only the
	 * values handed to the compositor are scaled to physical pixels. */
	float        pax = parent_node == XENT_NODE_INVALID ? 0.0f : vt->nodes [parent_node].abs_x;
	float        pay = parent_node == XENT_NODE_INVALID ? 0.0f : vt->nodes [parent_node].abs_y;
	WFN_Vector_3 off = {(rect->x - pax) * vt->scale, (rect->y - pay) * vt->scale, 0.0f};
	( void ) wuc_visual_put__offset(vn->visual, off);
	WFN_Vector_2 size = {rect->w * vt->scale, rect->h * vt->scale};
	( void ) wuc_visual_put__size(vn->visual, size);

	/* A scroll node clips to its viewport and routes its children through a content
	 * holder. The holder's Offset carries the scroll: an InteractionTracker drives it
	 * on the compositor when one is bound (see flux_compose_render); until then the
	 * offset is folded into the children's inherited base as a fallback. The node's
	 * own offset above is unaffected, so the scrollbar/background stay put. */
	if (flux_get_control_type(vt->ctx, node) == FLUX_CONTROL_SCROLL) {
		tree_clip_to_bounds(vt, vn);
		tree_make_holder(vt, vn);
	}
	float sx, sy;
	tree_node_scroll(vt, node, &sx, &sy);
	if (vn->tracker_bound) sx = sy = 0.0f; /* tracker owns the offset */

	vn->abs_x = rect->x + sx;
	vn->abs_y = rect->y + sy;
	vn->seen  = vt->generation;
	return vn->container;
}

static void tree_sweep(FluxVisualTree *vt) {
	for (uint32_t id = 0; id < vt->cap; id++) {
		VisNode *vn = &vt->nodes [id];
		if (!vn->in_use || vn->seen == vt->generation) continue;

		/* Detach from the parent only if the parent survived this pass; if the
		 * parent is also gone, releasing it detaches the whole subtree at once.
		 * Children live under the parent's holder when it is a scroll node. */
		bool parent_alive
		  = vn->parent == XENT_NODE_INVALID
		 || (vn->parent < vt->cap && vt->nodes [vn->parent].in_use && vt->nodes [vn->parent].seen == vt->generation);
		WUC_Container *pc = parent_alive ? tree_child_parent(vt, vn->parent) : NULL;
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
		vn->tracker_bound = false;
		vn->parent        = XENT_NODE_INVALID;
	}
}

typedef struct ReconcileFrame {
	XentNodeId node;
	XentNodeId parent;
} ReconcileFrame;

/* Push a frame onto the traversal stack, growing it on demand. False on OOM. */
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

/* Push every child of `parent` as a frame. False on OOM (caller should stop). */
static bool
reconcile_push_children(ReconcileFrame **stack, uint32_t *top, uint32_t *cap, XentContext *ctx, XentNodeId parent) {
	for (XentNodeId child = xent_get_first_child(ctx, parent); child != XENT_NODE_INVALID;
	  child               = xent_get_next_sibling(ctx, child))
		if (!reconcile_stack_push(stack, top, cap, (ReconcileFrame) {child, parent})) return false;
	return true;
}

void flux_visual_tree_reconcile(
  FluxVisualTree *vt, XentContext *ctx, FluxNodeStore *store, XentNodeId root, float scale
) {
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
		xent_get_layout_rect(ctx, f.node, &rect);

		if (!tree_sync_node(vt, f.node, f.parent, &rect)) continue;
		if (!reconcile_push_children(&stack, &top, &cap, ctx, f.node)) goto done;
	}

done:
	free(stack);
	tree_sweep(vt);
}

WUC_Container *flux_visual_tree_node_visual(FluxVisualTree *vt, XentNodeId node) {
	if (!vt || node >= vt->cap || !vt->nodes [node].in_use) return NULL;
	return vt->nodes [node].container;
}

WUC_Visual *flux_visual_tree_node_scroll_holder(FluxVisualTree *vt, XentNodeId node) {
	if (!vt || node >= vt->cap || !vt->nodes [node].in_use) return NULL;
	return vt->nodes [node].holder_visual;
}

void flux_visual_tree_set_tracker_bound(FluxVisualTree *vt, XentNodeId node, bool bound) {
	if (!vt || node >= vt->cap || !vt->nodes [node].in_use) return;
	vt->nodes [node].tracker_bound = bound;
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
