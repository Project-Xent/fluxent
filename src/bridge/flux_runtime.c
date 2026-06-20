/**
 * @file flux_runtime.c
 * @brief Fluxent app entry point: creates the backend, wires the xtk runtime,
 * and drives the Win32 message loop.
 */
#include "flux_internal.h"

static void flux_frame_cb(void *ctx) { xtk_runtime_frame(( XtkRuntime * ) ctx); }

int flux_run(FluxAppConfig const *cfg, void *model, XtkUpdateFn update, XtkViewFn view) {
	FluxApp *app = flux_app_create(cfg);
	if (!app) return 1;

	FluxBackendCtx bctx = {
	  .ctx     = flux_app_get_context(app),
	  .store   = flux_app_get_store(app),
	  .app     = app,
	  .runtime = NULL,
	};
	XtkBackend be = flux_xtk_backend(&bctx);

	XtkRuntime *rt = xtk_runtime_create(bctx.ctx, &be, flux_app_get_root(app), model, update, view);
	if (!rt) {
		flux_app_destroy(app);
		return 1;
	}
	bctx.runtime = rt;

	flux_app_set_frame_callback(app, flux_frame_cb, rt);
	int result = flux_app_run(app);

	flux_app_set_frame_callback(app, NULL, NULL);
	xtk_runtime_destroy(rt);
	flux_app_destroy(app);
	return result;
}
