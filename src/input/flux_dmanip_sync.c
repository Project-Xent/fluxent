#include "flux_dmanip_sync.h"
#include "store/flux_node_data.h"
#include "controls/data/flux_component_data.h"
#include "fluxent/flux_input.h"

#include <windows.h>

typedef struct DManipSyncContext {
	FluxDManip *dmanip;
	XentCtx    *ctx;
	FluxStore  *store;
	float       scale;
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
	bool changed = sd->scroll_x != sx || sd->scroll_y != sy;
	sd->scroll_x = sx;
	sd->scroll_y = sy;
	if (!changed) return;
	sd->user_moved = 1;
	flux_scroll_offset_changed(sd);
}

static FluxDManipViewport *ensure_scroll_viewport(FluxDManip *dmanip, FluxScrollData *sd) {
	if (sd->dmanip_viewport) return ( FluxDManipViewport * ) sd->dmanip_viewport;

	FluxDManipViewport *vp = NULL;
	if (FAILED(flux_dmanip_viewport_create(dmanip, &vp))) return NULL;

	flux_dmanip_vp_onchange(vp, dmanip_on_update, sd);
	sd->dmanip_viewport = vp;
	return vp;
}

static void sync_scroll_viewport(DManipSyncContext const *sync, FluxScrollData *sd, FluxRect const *viewport) {
	FluxDManipViewport *vp = ensure_scroll_viewport(sync->dmanip, sd);
	if (!vp) return;

	flux_dmanip_vp_setdpi(vp, sync->scale);

	int px = ( int ) (viewport->x * sync->scale + 0.5f);
	int py = ( int ) (viewport->y * sync->scale + 0.5f);
	int pw = ( int ) (viewport->w * sync->scale + 0.5f);
	int ph = ( int ) (viewport->h * sync->scale + 0.5f);
	flux_dmanip_vp_setrect(vp, px, py, pw, ph);

	int cw = ( int ) (sd->content_w * sync->scale + 0.5f);
	int ch = ( int ) (sd->content_h * sync->scale + 0.5f);
	if (cw < pw) cw = pw;
	if (ch < ph) ch = ph;
	flux_dmanip_vp_setsize(vp, cw, ch);
	flux_dmanip_vp_setsnapy(vp, sd->snap_y * sync->scale);

	flux_dmanip_vp_sync(vp, sd->scroll_x, sd->scroll_y);
}

static void sync_scroll_node(DManipSyncContext const *sync, DManipNodeState *state) {
	if (flux_node_type(sync->store, state->node) != FLUX_CONTROL_SCROLL) return;

	FluxNodeData   *nd = flux_store_find(sync->store, state->node);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!sd) return;

	sync_scroll_viewport(sync, sd, &state->viewport);
	state->child_ax -= flux_scroll_off_x(sd);
	state->child_ay -= flux_scroll_off_y(sd);
}

static void sync_node(DManipSyncContext const *sync, XentNodeId node, float ax_dip, float ay_dip) {
	if (node == XENT_NODE_INVALID) return;

	XentRect lr = {0};
	xent_layout_rect(sync->ctx, node, &lr);
	DManipNodeState state = {
	  .node     = node,
	  .viewport = {ax_dip + lr.x, ay_dip + lr.y, lr.w, lr.h},
	  .child_ax = ax_dip + lr.x,
	  .child_ay = ay_dip + lr.y,
	};

	sync_scroll_node(sync, &state);

	XentNodeId c = xent_node_first(sync->ctx, node);
	while (c != XENT_NODE_INVALID) {
		sync_node(sync, c, state.child_ax, state.child_ay);
		c = xent_node_next(sync->ctx, c);
	}
}

void flux_dmanip_sync_tree(FluxDManip *dmanip, XentCtx *ctx, FluxStore *store, XentNodeId root, float scale) {
	if (!dmanip || !ctx || !store || root == XENT_NODE_INVALID) return;
	DManipSyncContext sync = {dmanip, ctx, store, scale};
	sync_node(&sync, root, 0.0f, 0.0f);
}

static FluxDManipViewport *dmanip_viewport_for_input_pan(FluxInput *input, FluxStore *store, FluxScrollData **out_sd) {
	XentNodeId target = flux_input_pantarget(input);
	if (target == XENT_NODE_INVALID) return NULL;

	FluxNodeData   *nd = flux_store_find(store, target);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	*out_sd            = sd;
	return sd ? ( FluxDManipViewport * ) sd->dmanip_viewport : NULL;
}

bool flux_dmanip_pan_handoff(FluxDManip *dmanip, FluxInput *input, FluxStore *store, uint32_t pointer_id) {
	if (!dmanip || !input || !store || pointer_id == 0) return false;
	if (!flux_input_takepan(input)) return false;

	FluxScrollData     *sd = NULL;
	FluxDManipViewport *vp = dmanip_viewport_for_input_pan(input, store, &sd);
	if (!vp) return false;

	flux_dmanip_vp_sync(vp, sd->scroll_x, sd->scroll_y);
	if (FAILED(flux_dmanip_vp_setcontact(vp, pointer_id))) return false;

	flux_input_clearpan(input);
	return true;
}

static void cleanup_scroll_node(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	if (flux_node_type(store, node) != FLUX_CONTROL_SCROLL) return;

	FluxNodeData   *nd = flux_store_find(store, node);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!sd || !sd->dmanip_viewport) return;

	flux_dmanip_viewport_destroy(( FluxDManipViewport * ) sd->dmanip_viewport);
	sd->dmanip_viewport = NULL;
}

static void cleanup_node(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return;

	cleanup_scroll_node(ctx, store, node);

	XentNodeId c = xent_node_first(ctx, node);
	while (c != XENT_NODE_INVALID) {
		cleanup_node(ctx, store, c);
		c = xent_node_next(ctx, c);
	}
}

void flux_dmanip_cleanup_tree(XentCtx *ctx, FluxStore *store, XentNodeId root) {
	if (!ctx || !store || root == XENT_NODE_INVALID) return;
	cleanup_node(ctx, store, root);
}

void flux_dmanip_vp_release(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || nd->component_type != FLUX_CONTROL_SCROLL || !nd->component_data) return;

	FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
	if (!sd->dmanip_viewport) return;
	flux_dmanip_viewport_destroy(( FluxDManipViewport * ) sd->dmanip_viewport);
	sd->dmanip_viewport = NULL;
}
