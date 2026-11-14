#include "flux_internal.h"
#include "app/flux_post.h"
#include "render/flux_dl_replay.h"

#include <fluxent/flux_host.h>

static void be_request_frame(void *ctx) {
	FluxBackendCtx *rt = ( FluxBackendCtx * ) ctx;
	if (rt->app) flux_app_redraw(rt->app);
}

XtkRtCfg flux_xtk_runtime_config(FluxBackendCtx *bctx) {
	return (XtkRtCfg) {
	  .ctx           = bctx,
	  .request_frame = be_request_frame,
	};
}

static void frame_cb(void *ctx) { xtk_rt_frame(( XtkRt * ) ctx); }

XtkHost    *flux_app_attachhost(FluxApp *app) {
	if (!app) return NULL;
	FluxBackendCtx bctx = {
	  .ctx   = flux_app_ctx(app),
	  .store = flux_app_store(app),
	  .app   = app,
	};
	bctx.host = flux_host_adopt(bctx.ctx, bctx.store, flux_app_root(app));
	if (!bctx.host) return NULL;

	XtkBackend *backend = flux_xtk_backend_create(&bctx, bctx.host);
	if (!backend) {
		flux_host_destroy(bctx.host);
		return NULL;
	}
	flux_app_sethost(app, bctx.host, backend);
	return bctx.host;
}

XtkRt *flux_app_createruntime(FluxApp *app, void *model, XtkUpdateFn update, XtkViewFn view) {
	XtkBackend     *backend = flux_app_backend(app);
	FluxBackendCtx *bctx    = flux_xtk_backendctx(backend);
	if (!bctx) return NULL;

	XtkRtCfg rt_cfg = flux_xtk_runtime_config(bctx);
	XtkRt   *rt     = xtk_rt_create(bctx->host, &rt_cfg, flux_app_root(app), model, update, view);
	if (!rt) return NULL;
	if (!flux_app_bindruntime(app, rt)) {
		( void ) xtk_rt_destroy(rt);
		return NULL;
	}
	bctx->runtime = rt;
	flux_app_onframe(app, frame_cb, rt);
	return rt;
}

static bool teardown(FluxApp *app, XtkRt *rt, XtkHost *host) {
	bool ok = true;
	if (rt) ok = xtk_rt_destroy(rt);
	if (host) flux_host_destroy(host);
	flux_app_destroy(app);
	return ok;
}

int flux_run(FluxAppCfg const *cfg, void *model, XtkUpdateFn update, XtkViewFn view) {
	FluxApp *app = flux_app_create(cfg);
	if (!app) return 1;

	int      result = 1;
	XtkHost *host   = flux_app_attachhost(app);
	XtkRt   *rt     = host ? flux_app_createruntime(app, model, update, view) : NULL;
	if (rt) {
		if (cfg && cfg->on_start) cfg->on_start(app, cfg->user);
		result = flux_app_run(app);
		flux_app_onframe(app, NULL, NULL);
		if (cfg && cfg->on_stop) cfg->on_stop(app, cfg->user);
	}
	if (!teardown(app, rt, host)) result = 1;
	return result;
}
