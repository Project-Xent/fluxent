#include "controls/factory/flux_init_container.h"
#include "fluxent/flux_host.h"

#include <xtk/xtk_catalog.h>

#include "runtime/flux_anim_driver.h"
#include "fluxent/flux_window.h"

#include <stdlib.h>
#include <math.h>

void flux_scroll_measure(XentCtx const *ctx, XentNodeId node, FluxScrollData *sd) {
	XentRect rect = {0}, cr = {0};
	if (sd) sd->rtl = xent_resolved_dir(ctx, node) == XENT_DIRECTION_RTL;
	if (!sd || sd->content_manual || !xent_layout_rect(ctx, node, &rect)) return;
	float max_r = 0.0f, max_b = 0.0f;
	for (XentNodeId child = xent_node_first(ctx, node); child != XENT_NODE_INVALID; child = xent_node_next(ctx, child))
	{
		if (!xent_layout_rect(ctx, child, &cr)) continue;
		XentRect origin = {rect.x, rect.y, rect.w, 0.0f};
		max_r           = fmaxf(max_r, flux_scroll_start(sd, &cr, &origin) + cr.w);
		max_b           = fmaxf(max_b, cr.y + cr.h - rect.y);
	}
	XentResolvedInsets pad = {0};
	xent_resolved_p(ctx, node, XENT_AXIS_VERTICAL, &pad);
	sd->content_w = max_r + pad.cross_end;
	sd->content_h = max_b + pad.main_end;
}

void flux_scroll_data_destroy(void *data) {
	flux_anim_unregister(data);
	free(data);
}

static bool scroll_wheel_step(void *data, unsigned long now) {
	FluxScrollData *sd = data;
	if (!sd->wheel_active) return false;
	float t        = fminf(( float ) (now - sd->wheel_start) / 160.0f, 1.0f);
	float eased    = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
	sd->scroll_x   = sd->wheel_from_x + (sd->wheel_target_x - sd->wheel_from_x) * eased;
	sd->scroll_y   = sd->wheel_from_y + (sd->wheel_target_y - sd->wheel_from_y) * eased;
	sd->user_moved = 1;
	flux_scroll_offset_changed(sd);
	if (sd->wheel_window) flux_window_redraw(sd->wheel_window);
	sd->wheel_active = t < 1.0f;
	return sd->wheel_active;
}

void flux_scroll_wheel_animate(FluxScrollData *sd, FluxWindow *window, float x, float y) {
	sd->wheel_window   = window;
	sd->wheel_from_x   = sd->scroll_x;
	sd->wheel_from_y   = sd->scroll_y;
	sd->wheel_target_x = x;
	sd->wheel_target_y = y;
	sd->wheel_start    = flux_anim_now();
	sd->wheel_active   = 1;
	flux_anim_register(sd, scroll_wheel_step);
}

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
	if (!sd || !xent_layout_rect(ctx, node, &rect)) return false;
	flux_scroll_measure(ctx, node, sd);
	float max_x = scroll_max(sd->content_w, rect.w);
	float max_y = scroll_max(sd->content_h, rect.h);
	bool  moved = sd->reports && sd->user_moved;
	if (moved) sd->pin_end = 0;
	if (sd->pin_end) sd->scroll_y = max_y;
	sd->scroll_x = fminf(fmaxf(sd->scroll_x, 0.0f), max_x);
	sd->scroll_y = fminf(fmaxf(sd->scroll_y, 0.0f), max_y);
	if (!sd->reports) return false;
	sd->user_moved = 0;
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
