#include "controls/data/flux_image_data.h"
#include "controls/flux_module.h"
#include "render/flux_image.h"
#include "runtime/flux_time.h"
#include "fluxent/flux_host.h"
#include "store/flux_node_data.h"

#include <math.h>

static float image_scale(float avail, XentMeasureMode mode, float natural) {
	return mode == XENT_MEASURE_UNDEFINED || !isfinite(avail) ? INFINITY : avail / natural;
}

static void image_fit(FluxImageStretch stretch, float *sx, float *sy) {
	if (stretch == FLUX_IMAGE_NONE || (isinf(*sx) && isinf(*sy))) {
		*sx = *sy = 1.0f;
		return;
	}
	if (isinf(*sx) || isinf(*sy)) {
		*sx = *sy = isinf(*sx) ? *sy : *sx;
		return;
	}
	if (stretch == FLUX_IMAGE_FILL) return;
	*sx = *sy = stretch == FLUX_IMAGE_UNIFORM ? fminf(*sx, *sy) : fmaxf(*sx, *sy);
}

static bool image_measure(void *userdata, XentExtMeasureReq const *rq, XentSize *out) {
	FluxImageData const *im = ( FluxImageData const * ) userdata;
	float                w = 0.0f, h = 0.0f;
	*out = (XentSize) {0.0f, 0.0f};
	if (!flux_image_size(im->image, &w, &h) || w <= 0.0f || h <= 0.0f) return true;
	float sx = image_scale(rq->available_w, rq->width_mode, w);
	float sy = image_scale(rq->available_h, rq->height_mode, h);
	image_fit(im->stretch, &sx, &sy);
	*out = (XentSize) {w * sx, h * sy};
	return true;
}

void flux_image_bind(XentCtx *ctx, XentNodeId node, FluxImageData *im) {
	im->image  = flux_image_get(im->source);
	im->seen_w = im->seen_h = 0.0f;
	im->start               = 0.0;
	im->reported            = 0u;
	xent_node_setmeasure(ctx, node, image_measure, im);
}

static void image_report(FluxStore *store, XentNodeId node, FluxImageData *im) {
	bool     failed = flux_image_failed(im->image);
	XtkHost *host   = flux_store_host(store);
	if (im->reported || !(failed || flux_image_ready(im->image))) return;
	im->reported = failed ? 2u : 1u;
	if (!host) return;
	XtkEvent event = {
	  .id             = failed ? XTK_EVENT_IMAGE_FAILED : XTK_EVENT_IMAGE_OPENED,
	  .target         = ( XtkCtrl ) node,
	  .source         = ( XtkCtrl ) node,
	  .current_target = ( XtkCtrl ) node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_PROGRAM,
	};
	flux_host_route(host, &event);
}

uint32_t flux_image_sync(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) {
	FluxImageData *im = ( FluxImageData * ) nd->component_data;
	float          w = 0.0f, h = 0.0f;
	double         now = flux_perf_seconds(flux_perf_now());
	if (flux_image_size(im->image, &w, &h) && (w != im->seen_w || h != im->seen_h)) {
		im->seen_w = w;
		im->seen_h = h;
		xent_node_setmeasure(ctx, node, image_measure, im);
	}
	if (flux_image_ready(im->image) && im->start == 0.0) im->start = now;
	image_report(store, node, im);
	im->frame = flux_image_frame(im->image, now - im->start);
	return flux_image_playing(im->image, now - im->start) ? FLUX_FRAME_REDRAW : 0u;
}
