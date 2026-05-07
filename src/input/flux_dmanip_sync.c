#include "flux_dmanip_sync.h"
#include "fluxent/flux_component_data.h"
#include "fluxent/flux_input.h"

#include <windows.h>

typedef struct DManipSyncContext {
	FluxDManip    *dmanip;
	XentContext   *ctx;
	FluxNodeStore *store;
	float          scale;
} DManipSyncContext;

typedef struct DManipNodeState {
	XentNodeId node;
	FluxRect   viewport;
	float      child_ax;
	float      child_ay;
} DManipNodeState;

static void dmanip_on_update(void *ctx, float sx, float sy) {
	FluxScrollData *sd = ( FluxScrollData * ) ctx;
	if (!sd) return;
	sd->scroll_x = sx;
	sd->scroll_y = sy;
}

static FluxDManipViewport *ensure_scroll_viewport(FluxDManip *dmanip, FluxScrollData *sd) {
	if (sd->dmanip_viewport) return ( FluxDManipViewport * ) sd->dmanip_viewport;

	FluxDManipViewport *vp = NULL;
	if (FAILED(flux_dmanip_viewport_create(dmanip, &vp))) return NULL;

	flux_dmanip_viewport_set_callback(vp, dmanip_on_update, sd);
	sd->dmanip_viewport = vp;
	return vp;
}

static void sync_scroll_viewport(DManipSyncContext const *sync, FluxScrollData *sd, FluxRect const *viewport) {
	FluxDManipViewport *vp = ensure_scroll_viewport(sync->dmanip, sd);
	if (!vp) return;

	flux_dmanip_viewport_set_dpi_scale(vp, sync->scale);

	int px = ( int ) (viewport->x * sync->scale + 0.5f);
	int py = ( int ) (viewport->y * sync->scale + 0.5f);
	int pw = ( int ) (viewport->w * sync->scale + 0.5f);
	int ph = ( int ) (viewport->h * sync->scale + 0.5f);
	flux_dmanip_viewport_set_rect(vp, px, py, pw, ph);

	int cw = ( int ) (sd->content_w * sync->scale + 0.5f);
	int ch = ( int ) (sd->content_h * sync->scale + 0.5f);
	if (cw < pw) cw = pw;
	if (ch < ph) ch = ph;
	flux_dmanip_viewport_set_content_size(vp, cw, ch);

	flux_dmanip_viewport_sync_transform(vp, sd->scroll_x, sd->scroll_y);
}

static void sync_scroll_node(DManipSyncContext const *sync, DManipNodeState *state) {
	if (xent_get_control_type(sync->ctx, state->node) != XENT_CONTROL_SCROLL) return;

	FluxNodeData   *nd = flux_node_store_get(sync->store, state->node);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!sd) return;

	sync_scroll_viewport(sync, sd, &state->viewport);
	state->child_ax -= sd->scroll_x;
	state->child_ay -= sd->scroll_y;
}

static void sync_node(DManipSyncContext const *sync, XentNodeId node, float ax_dip, float ay_dip) {
	if (node == XENT_NODE_INVALID) return;

	XentRect lr = {0};
	xent_get_layout_rect(sync->ctx, node, &lr);
	DManipNodeState state = {
	  .node     = node,
	  .viewport = {ax_dip + lr.x, ay_dip + lr.y, lr.width, lr.height},
	  .child_ax = ax_dip + lr.x,
	  .child_ay = ay_dip + lr.y,
	};

	sync_scroll_node(sync, &state);

	XentNodeId c = xent_get_first_child(sync->ctx, node);
	while (c != XENT_NODE_INVALID) {
		sync_node(sync, c, state.child_ax, state.child_ay);
		c = xent_get_next_sibling(sync->ctx, c);
	}
}

void flux_dmanip_sync_tree(FluxDManip *dmanip, XentContext *ctx, FluxNodeStore *store, XentNodeId root, float scale) {
	if (!dmanip || !ctx || !store || root == XENT_NODE_INVALID) return;
	DManipSyncContext sync = {dmanip, ctx, store, scale};
	sync_node(&sync, root, 0.0f, 0.0f);
}

static FluxDManipViewport *
dmanip_viewport_for_input_pan(FluxInput *input, FluxNodeStore *store, FluxScrollData **out_sd) {
	XentNodeId target = flux_input_get_touch_pan_target(input);
	if (target == XENT_NODE_INVALID) return NULL;

	FluxNodeData   *nd = flux_node_store_get(store, target);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	*out_sd            = sd;
	return sd ? ( FluxDManipViewport * ) sd->dmanip_viewport : NULL;
}

bool flux_dmanip_handoff_touch_pan(FluxDManip *dmanip, FluxInput *input, FluxNodeStore *store, uint32_t pointer_id) {
	if (!dmanip || !input || !store || pointer_id == 0) return false;
	if (!flux_input_take_pan_promoted(input)) return false;

	FluxScrollData     *sd = NULL;
	FluxDManipViewport *vp = dmanip_viewport_for_input_pan(input, store, &sd);
	if (!vp) return false;

	flux_dmanip_viewport_sync_transform(vp, sd->scroll_x, sd->scroll_y);
	if (FAILED(flux_dmanip_viewport_set_contact(vp, pointer_id))) return false;

	flux_input_clear_touch_pan(input);
	return true;
}

static void cleanup_scroll_node(XentContext *ctx, FluxNodeStore *store, XentNodeId node) {
	if (xent_get_control_type(ctx, node) != XENT_CONTROL_SCROLL) return;

	FluxNodeData   *nd = flux_node_store_get(store, node);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!sd || !sd->dmanip_viewport) return;

	flux_dmanip_viewport_destroy(( FluxDManipViewport * ) sd->dmanip_viewport);
	sd->dmanip_viewport = NULL;
}

static void cleanup_node(XentContext *ctx, FluxNodeStore *store, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return;

	cleanup_scroll_node(ctx, store, node);

	XentNodeId c = xent_get_first_child(ctx, node);
	while (c != XENT_NODE_INVALID) {
		cleanup_node(ctx, store, c);
		c = xent_get_next_sibling(ctx, c);
	}
}

void flux_dmanip_cleanup_tree(XentContext *ctx, FluxNodeStore *store, XentNodeId root) {
	if (!ctx || !store || root == XENT_NODE_INVALID) return;
	cleanup_node(ctx, store, root);
}
