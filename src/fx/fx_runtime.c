/**
 * @file fx_runtime.c
 * @brief FX runtime: message queue, frame pump, and the fx_app_run loop glue.
 */
#include "fx_internal.h"

#include <stdlib.h>

void fx_runtime_post(FxRuntime *rt, FxMsg msg) {
	if (!rt || msg.tag == 0) return;
	int next = (rt->q_tail + 1) % FX_QUEUE_CAP;
	if (next == rt->q_head) return;
	rt->queue [rt->q_tail] = msg;
	rt->q_tail             = next;
	if (rt->app) flux_app_request_render(rt->app);
}

static bool fx_runtime_take(FxRuntime *rt, FxMsg *out) {
	if (rt->q_head == rt->q_tail) return false;
	*out       = rt->queue [rt->q_head];
	rt->q_head = (rt->q_head + 1) % FX_QUEUE_CAP;
	return true;
}

/* One pump: drain messages into update, then rebuild + reconcile the view.
 * Skipped entirely when nothing happened (animation frames stay cheap). */
void fx_runtime_frame(FxRuntime *rt) {
	bool  dirty = rt->q_head != rt->q_tail || !rt->root;

	FxMsg msg;
	while (fx_runtime_take(rt, &msg)) rt->update(rt->model, msg);
	if (!dirty) return;

	fx_ui_begin(rt->ui);
	fx_reconcile(rt, rt->view(rt->ui, rt->model));
}

FxRuntime *fx_runtime_create(
  XentContext *ctx, FluxNodeStore *store, XentNodeId host, FluxApp *app, void *model, void (*update)(void *, FxMsg),
  FluxEl *(*view)( FxUi *, void * )
) {
	if (!ctx || !store || host == XENT_NODE_INVALID || !update || !view) return NULL;

	FxRuntime *rt = ( FxRuntime * ) calloc(1, sizeof(*rt));
	if (!rt) return NULL;

	rt->ui = fx_ui_create();
	if (!rt->ui) {
		free(rt);
		return NULL;
	}

	rt->ctx    = ctx;
	rt->store  = store;
	rt->host   = host;
	rt->app    = app;
	rt->model  = model;
	rt->update = update;
	rt->view   = view;
	return rt;
}

void fx_runtime_destroy(FxRuntime *rt) {
	if (!rt) return;
	fx_unmount_all(rt);
	fx_ui_destroy(rt->ui);
	free(rt);
}

static void fx_frame_cb(void *ctx) { fx_runtime_frame(( FxRuntime * ) ctx); }

int         fx_app_run(FluxAppConfig const *cfg, void *model, FxUpdateFn update, FxViewFn view) {
	FluxApp *app = flux_app_create(cfg);
	if (!app) return 1;

	FxRuntime *rt = fx_runtime_create(
	  flux_app_get_context(app), flux_app_get_store(app), flux_app_get_root(app), app, model, update, view
	);
	if (!rt) {
		flux_app_destroy(app);
		return 1;
	}

	flux_app_set_frame_callback(app, fx_frame_cb, rt);
	int result = flux_app_run(app);

	flux_app_set_frame_callback(app, NULL, NULL);
	fx_runtime_destroy(rt);
	flux_app_destroy(app);
	return result;
}
