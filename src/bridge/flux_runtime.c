/**
 * @file flux_runtime.c
 * @brief Fluxent app entry point: XtkRtCfg glue, backend/runtime wiring,
 * and the Win32 message loop.
 */
#include "flux_internal.h"
#include "render/flux_dl_replay.h"

#include <fluxent/flux_host.h>

static void be_request_frame(void *ctx) {
	FluxBackendCtx *rt = ( FluxBackendCtx * ) ctx;
	if (rt->app) flux_app_redraw(rt->app);
}

static void list_stale(void *ctx) {
	FluxBackendCtx *rt = ( FluxBackendCtx * ) ctx;
	if (rt->runtime) xtk_rt_invalidate(rt->runtime);
}

static void ensure_list_stale(FluxBackendCtx *rt, XentNodeId id) { flux_list_onstale(rt->store, id, list_stale, rt); }

static bool be_viewport(void *ctx, XentNodeId id, XtkViewport *out) {
	FluxBackendCtx *rt = ( FluxBackendCtx * ) ctx;
	ensure_list_stale(rt, id);
	flux_list_apply_pending(rt->store, id);
	return flux_list_viewport(rt->store, id, &out->offset, &out->extent, &out->cross);
}

static void be_virtual_range(void *ctx, XentNodeId id, XtkVirtualRange const *range) {
	FluxBackendCtx *rt = ( FluxBackendCtx * ) ctx;
	if (id == XENT_NODE_INVALID) return;
	ensure_list_stale(rt, id);
	flux_list_stage_realized(rt->store, id, range->first, range->last, range->lanes);
	/* Expanding windows apply after Host commit creates children. Same-size
	 * shifts (and cols-only updates) often produce an empty batch — apply now. */
	XentNodeId host = flux_list_content_node(rt->store, id);
	if (host == XENT_NODE_INVALID) return;
	int n      = xent_node_nchild(flux_store_ctx(rt->store), host);
	int window = range->last >= range->first ? range->last - range->first + 1 : 0;
	if (n >= window) flux_list_apply_pending(rt->store, id);
}

XtkRtCfg flux_xtk_runtime_config(FluxBackendCtx *bctx) {
	return (XtkRtCfg) {
	  .ctx           = bctx,
	  .request_frame = be_request_frame,
	  .viewport      = be_viewport,
	  .virtual_range = be_virtual_range,
	};
}

static void frame_cb(void *ctx) { xtk_rt_frame(( XtkRt * ) ctx); }

/* Reverse-dependency teardown, always run to completion: the runtime uses the
 * host, the host borrows the app's ctx/store/root (adopt), and the app owns the
 * backend since flux_app_sethost. False means the runtime refused to
 * destroy; host and app are still torn down. */
static bool teardown(FluxApp *app, FluxBackendCtx *bctx) {
	bool ok = true;
	if (bctx->runtime) ok = xtk_rt_destroy(bctx->runtime);
	if (bctx->host) flux_host_destroy(bctx->host);
	flux_app_destroy(app);
	return ok;
}

int flux_run(FluxAppCfg const *cfg, void *model, XtkUpdateFn update, XtkViewFn view) {
	FluxApp *app = flux_app_create(cfg);
	if (!app) return 1;

	FluxBackendCtx bctx = {
	  .ctx   = flux_app_ctx(app),
	  .store = flux_app_store(app),
	  .app   = app,
	};
	int result = 1;

	bctx.host  = flux_host_adopt(bctx.ctx, bctx.store, flux_app_root(app));
	if (!bctx.host) goto teardown;

	XtkBackend *backend = flux_xtk_backend_create(&bctx, bctx.host);
	if (!backend) goto teardown;
	flux_app_sethost(app, bctx.host, backend);

	XtkRtCfg rt_cfg = flux_xtk_runtime_config(&bctx);
	bctx.runtime    = xtk_rt_create(bctx.host, &rt_cfg, flux_app_root(app), model, update, view);
	if (!bctx.runtime) goto teardown;

	flux_app_onframe(app, frame_cb, bctx.runtime);
	result = flux_app_run(app);
	flux_app_onframe(app, NULL, NULL);

teardown:
	if (!teardown(app, &bctx)) result = 1;
	return result;
}
