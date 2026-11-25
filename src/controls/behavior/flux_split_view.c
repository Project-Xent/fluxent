#include "controls/flux_control_uia.h"
#include "bridge/flux_internal.h"
#include "controls/factory/flux_factory.h"
#include "input/flux_input_internal.h"
#include "store/flux_node_data.h"
#include "controls/factory/flux_init_container.h"

#include <math.h>
#include <stdlib.h>

static FluxSplitViewData *split_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_SPLIT_VIEW || !nd->component_data) return NULL;
	return ( FluxSplitViewData * ) nd->component_data;
}

static void split_clear_dismiss(FluxSplitViewData *d) {
	if (d->dismiss_pane == XENT_NODE_INVALID) return;
	d->dismiss_pane = XENT_NODE_INVALID;
	flux_input_set_dismiss_layer(flux_store_input(d->store), d->node, XENT_NODE_INVALID);
}

static void split_restore_focus(FluxSplitViewData const *d) {
	FluxInput *input = flux_store_input(d->store);
	if (!input || !input_within(d->ctx, flux_input_focus(input), d->dismiss_pane)) return;
	if (flux_input_focuswithin(input, d->previous_focus, true)) return;
	if (flux_input_focuswithin(input, xent_node_first(d->ctx, d->node), true)) return;
	flux_input_setfocus(input, XENT_NODE_INVALID, true);
}

static void split_light_dismiss(FluxStore *store, XentNodeId node) {
	FluxSplitViewData *d = split_data(store, node);
	if (!d || !d->pane_open || !flux_splitview_is_overlay(d->display_mode)) return;
	d->pane_open = false;
	split_restore_focus(d);
	d = split_data(store, node);
	if (!d || d->pane_open) return;
	split_clear_dismiss(d);
	XtkHost *host = flux_store_host(store);
	if (host)
		flux_input_route_event(
		  host, node, XTK_EVENT_OPEN, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL}, XTK_EVENT_CAUSE_USER
		);
}

static void split_sync_dismiss(FluxSplitViewData *d, XentNodeId pane, bool active) {
	if (!active || pane == XENT_NODE_INVALID) {
		split_clear_dismiss(d);
		return;
	}
	if (d->dismiss_pane == pane) return;
	FluxInput *input = flux_store_input(d->store);
	if (!flux_input_set_dismiss_action(input, d->node, pane, split_light_dismiss)) return;
	d->dismiss_pane    = pane;
	XentNodeId focused = flux_input_focus(input);
	d->previous_focus  = input_within(d->ctx, focused, pane) ? XENT_NODE_INVALID : focused;
}

static void split_wrap_layout(XentCtx *ctx, XentNodeId n) {
	xent_setproto(ctx, n, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, n, XENT_FLEX_COLUMN);
	xent_setitems(ctx, n, XENT_FLEX_ALIGN_STRETCH);
}

XentNodeId flux_splitbody_init(XentCtx *ctx, FluxStore *store, XentNodeId parent) {
	XentNodeId n = flux_factory_create_node(ctx, store, parent, FLUX_CONTROL_SPLIT_VIEW_CONTENT);
	if (n != XENT_NODE_INVALID) split_wrap_layout(ctx, n);
	return n;
}

XentNodeId flux_splitpane_init(XentCtx *ctx, FluxStore *store, XentNodeId parent) {
	XentNodeId n = flux_factory_create_node(ctx, store, parent, FLUX_CONTROL_SPLIT_VIEW_PANE);
	if (n == XENT_NODE_INVALID) return n;

	FluxNodeData      *nd = flux_store_find(store, n);
	FluxSplitPaneData *pd = nd ? ( FluxSplitPaneData * ) calloc(1, sizeof(*pd)) : NULL;
	if (!nd || !pd) {
		free(pd);
		return flux_factory_fail_node(ctx, store, n);
	}
	nd->component_data         = pd;
	nd->destroy_component_data = free;
	nd->clips_children         = true;
	split_wrap_layout(ctx, n);
	return n;
}

XentNodeId flux_splitview_init(FluxSplitViewDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SPLIT_VIEW);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData      *nd = flux_store_find(info->store, node);
	FluxSplitViewData *d  = nd ? ( FluxSplitViewData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	d->ctx             = info->ctx;
	d->store           = info->store;
	d->node            = node;
	d->display_mode    = ( uint8_t ) info->display_mode;
	d->placement       = ( uint8_t ) info->pane_placement;
	d->pane_open       = info->pane_open;
	d->open_len        = info->open_pane_length > 0.0f ? info->open_pane_length : FLUX_SPLITVIEW_OPEN_LEN;
	d->compact_len     = info->compact_pane_length > 0.0f ? info->compact_pane_length : FLUX_SPLITVIEW_COMPACT_LEN;

	nd->component_data = d;
	nd->destroy_component_data = free;
	nd->clips_children         = true;

	xent_setproto(info->ctx, node, XENT_PROTOCOL_ABSOLUTE);
	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_CONTAINER);
	return node;
}

typedef struct SplitGeometry {
	float content_x;
	float content_w;
	float pane_x;
	float pane_w;
	bool  overlay;
} SplitGeometry;

static SplitGeometry split_geometry(FluxSplitViewData const *d, float width) {
	bool  compact = flux_splitview_is_compact(d->display_mode);
	float closed  = compact ? d->compact_len : 0.0f;
	float pane_w  = d->pane_open ? d->open_len : closed;
	if (pane_w > width) pane_w = width;
	bool  overlay = flux_splitview_is_overlay(d->display_mode);
	float inset   = overlay ? closed : pane_w;
	if (d->placement == FLUX_SPLITVIEW_RIGHT)
		return (SplitGeometry) {0.0f, fmaxf(0.0f, width - inset), width - pane_w, pane_w, overlay};
	return (SplitGeometry) {inset, fmaxf(0.0f, width - inset), 0.0f, pane_w, overlay};
}

static FluxSplitPaneData *split_pane_data(FluxSplitViewData *d, XentNodeId pane) {
	FluxNodeData *nd = flux_store_find(d->store, pane);
	if (!nd) return NULL;
	if (!nd->component_data) {
		FluxSplitPaneData *data = ( FluxSplitPaneData * ) calloc(1, sizeof(*data));
		if (!data) return NULL;
		nd->component_data         = data;
		nd->destroy_component_data = free;
		nd->clips_children         = true;
	}
	return ( FluxSplitPaneData * ) nd->component_data;
}

static void
split_layout_pane(XentCtx *ctx, FluxSplitViewData *d, XentNodeId pane, SplitGeometry const *g, float height) {
	if (pane == XENT_NODE_INVALID) return;
	xent_setpos(ctx, pane, (XentPoint) {g->pane_x, 0.0f});
	xent_setsize(ctx, pane, (XentSize) {g->pane_w, height});
	xent_setcollapsed(ctx, pane, g->pane_w <= 0.0f);
	FluxSplitPaneData *pd = split_pane_data(d, pane);
	if (!pd) return;
	pd->overlay      = g->overlay;
	pd->open         = d->pane_open;
	pd->placement    = d->placement;
	pd->divider      = g->pane_w > 0.5f;
	FluxNodeData *nd = flux_store_find(d->store, pane);
	nd->tab_cycle    = pd->overlay && pd->open;
}

void flux_splitview_sync(XentCtx *ctx, XentNodeId node, struct FluxNodeData *nd) {
	FluxSplitViewData *d = ( FluxSplitViewData * ) nd->component_data;
	XentRect           r = {0};
	if (!d || !xent_layout_rect(ctx, node, &r) || r.w <= 0.0f || r.h <= 0.0f) return;

	XentNodeId content = xent_node_first(ctx, node);
	if (content == XENT_NODE_INVALID) {
		split_clear_dismiss(d);
		return;
	}
	XentNodeId    pane = xent_node_next(ctx, content);
	SplitGeometry g    = split_geometry(d, r.w);
	xent_setpos(ctx, content, (XentPoint) {g.content_x, 0.0f});
	xent_setsize(ctx, content, (XentSize) {g.content_w, r.h});
	split_layout_pane(ctx, d, pane, &g, r.h);
	split_sync_dismiss(d, pane, g.overlay && d->pane_open && g.pane_w > 0.0f);
}

void flux_splitview_setopen(FluxStore *store, XentNodeId id, bool open) {
	FluxSplitViewData *d = split_data(store, id);
	if (!d) return;
	d->pane_open = open;
	if (!open) split_clear_dismiss(d);
}

void flux_splitview_configure(
  FluxStore *store, XentNodeId id, int display_mode, int placement, float open_len, float compact_len
) {
	FluxSplitViewData *d = split_data(store, id);
	if (!d) return;
	d->display_mode = ( uint8_t ) display_mode;
	if (!flux_splitview_is_overlay(d->display_mode)) split_clear_dismiss(d);
	d->placement = ( uint8_t ) placement;
	if (open_len > 0.0f) d->open_len = open_len;
	if (compact_len > 0.0f) d->compact_len = compact_len;
}

bool flux_splitview_uia_admits(FluxStore *store, XentNodeId node, XentNodeId child) {
	( void ) node;
	FluxNodeData            *nd = flux_store_find(store, child);
	FluxSplitPaneData const *pd = nd && nd->component_type == FLUX_CONTROL_SPLIT_VIEW_PANE ? nd->component_data : NULL;
	return !pd || pd->divider;
}

unsigned flux_splitpane_uia_live(FluxStore *store, XentNodeId node) {
	FluxNodeData            *nd = flux_store_find(store, node);
	FluxSplitPaneData const *pd = nd && nd->component_type == FLUX_CONTROL_SPLIT_VIEW_PANE ? nd->component_data : NULL;
	return pd && pd->open && pd->overlay ? ~0u : 0u;
}
