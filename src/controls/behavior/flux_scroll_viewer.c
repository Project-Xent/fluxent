#include "controls/factory/flux_init_container.h"
#include "fluxent/flux_host.h"

#include <xtk/xtk_catalog.h>

#include <math.h>

static FluxScrollData *reporting_scroll(FluxStore *store, XentNodeId node) {
	FluxNodeData   *nd = flux_store_find(store, node);
	FluxScrollData *sd
	  = nd && nd->component_type == FLUX_CONTROL_SCROLL ? ( FluxScrollData * ) nd->component_data : NULL;
	return sd && sd->reports ? sd : NULL;
}

static float scroll_max(float content, float viewport) { return content > viewport ? content - viewport : 0.0f; }

static bool  scroll_target(float requested, float reported) {
	return !isnan(requested) && !isinf(requested) && requested != reported;
}

void flux_scroll_request(FluxStore *store, XentNodeId node, float x, float y) {
	FluxScrollData *sd = reporting_scroll(store, node);
	if (!sd) return;
	if (!isnan(y)) sd->pin_end = isinf(y) && y > 0.0f;
	if (scroll_target(x, sd->reported_x)) sd->scroll_x = sd->reported_x = x;
	if (scroll_target(y, sd->reported_y)) sd->scroll_y = sd->reported_y = y;
	flux_scroll_offset_changed(sd);
}

bool flux_scroll_sync(XentCtx *ctx, XentNodeId node, FluxNodeData *nd) {
	FluxScrollData *sd   = ( FluxScrollData * ) nd->component_data;
	XentRect        rect = {0};
	if (!sd || !sd->reports || !xent_layout_rect(ctx, node, &rect)) return false;
	bool moved     = sd->user_moved;
	sd->user_moved = 0;
	float max_x    = scroll_max(sd->content_w, rect.w);
	float max_y    = scroll_max(sd->content_h, rect.h);
	if (moved) sd->pin_end = 0;
	if (sd->pin_end) sd->scroll_y = max_y;
	sd->scroll_x   = fminf(fmaxf(sd->scroll_x, 0.0f), max_x);
	sd->scroll_y   = fminf(fmaxf(sd->scroll_y, 0.0f), max_y);
	sd->reported_x = sd->scroll_x;
	sd->reported_y = sd->scroll_y;
	sd->max_x      = max_x;
	sd->max_y      = max_y;
	return moved;
}

void flux_scroll_report(FluxStore *store, XentNodeId node) {
	FluxScrollData *sd   = reporting_scroll(store, node);
	XtkHost        *host = flux_store_host(store);
	if (!sd || !host) return;
	XtkEvent event = {
	  .id             = XTK_EVENT_SCROLL,
	  .target         = ( XtkCtrl ) node,
	  .source         = ( XtkCtrl ) node,
	  .current_target = ( XtkCtrl ) node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	  .payload        = {
		.kind     = XTK_EVENT_PAYLOAD_SCROLL,
		.u.scroll = {sd->scroll_x, sd->scroll_y, sd->max_x, sd->max_y},
      },
	};
	flux_host_route(host, &event);
}
