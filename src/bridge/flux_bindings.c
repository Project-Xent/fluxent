/**
 * @file flux_bindings.c
 * @brief Host event routing for pointer/keyboard activation (no XtkEl path).
 *
 * Declarative subscriptions are kit-owned (SUBSCRIBE + trampoline). This file
 * only turns control activation into standard Host events.
 */
#include "flux_internal.h"

#include <controls/data/flux_checkbox_data.h>
#include <controls/data/flux_nav_view_data.h>
#include <fluxent/flux_host.h>
#include <fluxent/flux_node_store.h>
#include "store/flux_node_data.h"
#include <fluxent/flux_nav_trace.h>
#include <xtk/xtk_catalog.h>

static bool check_control(FluxControlType type) {
	return type == FLUX_CONTROL_CHECKBOX || type == FLUX_CONTROL_RADIO || type == FLUX_CONTROL_SWITCH;
}

typedef struct ActivationState {
	XentNodeId nav_root;
	int        nav_index;
	uint64_t   nav_revision;
	bool       handled;
} ActivationState;

void flux_input_route_event(
  XtkHost *host, XentNodeId node, XtkEventId id, XtkEventPayload payload, XtkEventCause cause
) {
	if (!host || node == XENT_NODE_INVALID) return;
	XtkEvent event = {
	  .id             = id,
	  .target         = ( XtkCtrl ) node,
	  .source         = ( XtkCtrl ) node,
	  .current_target = ( XtkCtrl ) node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = cause,
	  .trace_id       = flux_nav_trace_current(),
	  .payload        = payload,
	};
	flux_host_route(host, &event);
}

static void route_check_activation(XtkHost *host, XentNodeId node, FluxNodeData *data, XtkEventCause cause) {
	FluxCheckboxData *check = data ? ( FluxCheckboxData * ) data->component_data : NULL;
	bool checked = check ? check->state == FLUX_CHECK_CHECKED : xent_sem_checked(flux_host_context(host), node) != 0;
	if (check) {
		xent_sem_setchecked(flux_host_context(host), node, checked ? 1 : 0);
		if (check->on_change) check->on_change(check->on_change_ctx, check->state);
	}
	flux_input_route_event(
	  host, node, XTK_EVENT_CHANGE_BOOL, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = checked}, cause
	);
}

void flux_input_route_invoke(XtkHost *host, XentNodeId node, XtkEventCause cause) {
	flux_nav_trace_log("host.event", "invoke node=%llu cause=%d", ( unsigned long long ) node, cause);
	flux_input_route_event(host, node, XTK_EVENT_INVOKE, (XtkEventPayload) {0}, cause);
}

static void route_nav_select(XtkHost *host, XentNodeId root, XentNodeId item, int index, XtkEventCause cause) {
	XtkEvent event = {
	  .id             = XTK_EVENT_SELECT,
	  .target         = ( XtkCtrl ) root,
	  .source         = ( XtkCtrl ) root,
	  .current_target = ( XtkCtrl ) root,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = cause,
	  .trace_id       = flux_nav_trace_current(),
	  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index},
	};
	flux_nav_trace_log(
	  "host.event", "nav select root=%llu item=%llu index=%d cause=%d", ( unsigned long long ) root,
	  ( unsigned long long ) item, index, cause
	);
	flux_host_route(host, &event);
}

static FluxNavViewItem *activation_nav_item(FluxNodeData *data) {
	if (!data || data->component_type != FLUX_CONTROL_NAV_VIEW_ITEM) return NULL;
	return ( FluxNavViewItem * ) data->component_data;
}

static ActivationState activation_state(FluxNodeData *data) {
	FluxNavViewItem *item = activation_nav_item(data);
	return (ActivationState) {
	  .nav_root     = item && item->nav ? item->nav->root : XENT_NODE_INVALID,
	  .nav_index    = item ? item->index : -1,
	  .nav_revision = item && item->nav ? item->nav->selection_revision : 0u,
	  .handled      = data && data->behavior.on_click,
	};
}

static bool
activation_route_nav(XtkHost *host, FluxNodeData *data, ActivationState const *before, XtkEventCause cause) {
	FluxNavViewItem *item = activation_nav_item(data);
	if (!item
		|| !item->nav
		|| item->nav->root != before->nav_root
		|| item->index != before->nav_index
		|| item->nav->selection_revision == before->nav_revision)
		return false;
	route_nav_select(host, before->nav_root, item->node, before->nav_index, cause);
	return true;
}

static void activation_trace(XentNodeId node, FluxNodeData *data, ActivationState const *state) {
	flux_nav_trace_log(
	  "activation", "node=%llu type=%d handled=%d nav_index=%d nav_revision=%llu", ( unsigned long long ) node,
	  data ? data->component_type : -1, state->handled, state->nav_index, ( unsigned long long ) state->nav_revision
	);
}

void flux_input_route_activation(XtkHost *host, FluxStore *store, XentNodeId node, XtkEventCause cause) {
	if (!host || !store || node == XENT_NODE_INVALID) return;
	if (!flux_nav_trace_current())
		flux_nav_trace_begin("activation-api node=%llu cause=%d", ( unsigned long long ) node, cause);
	FluxNodeData   *data   = flux_store_find(store, node);
	ActivationState before = activation_state(data);
	activation_trace(node, data, &before);
	if (before.handled) data->behavior.on_click(data->behavior.on_click_ctx);
	data = flux_store_find(store, node);
	if (activation_route_nav(host, data, &before, cause)) return;
	if (check_control(flux_node_type(store, node))) {
		route_check_activation(host, node, data, cause);
		return;
	}
	if (!before.handled) flux_input_route_invoke(host, node, cause);
}
