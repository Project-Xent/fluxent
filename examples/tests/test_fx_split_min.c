#include "flux_internal.h"
#include <fluxent/flux_host.h>
#include <stdio.h>

static void update(void *m, XtkMsg msg) {
	( void ) m;
	( void ) msg;
}

static XtkEl *view(XtkUi *ui, void *m) {
	( void ) m;
	return xtk_split(
	  ui, "Save", (XtkSplitDesc) {.icon = "Save", .items = (XtkMenuItemDesc []) {{.label = "a"}}, .nitem = 1}
	);
}

int main(void) {
	XentCfg    c     = {0};
	XentCtx   *ctx   = xent_ctx_create(&c);
	FluxStore *store = flux_store_create(16);
	flux_store_bind(store, ctx);
	XentNodeId     host = xent_node_create(ctx);
	FluxBackendCtx bctx = {.ctx = ctx, .store = store};
	XtkHost       *h    = flux_host_adopt(ctx, store, host);
	bctx.host           = h;
	XtkRtCfg cfg        = flux_xtk_runtime_config(&bctx);
	int      m          = 0;
	XtkRt   *rt         = xtk_rt_create(h, &cfg, host, &m, update, view);
	bctx.runtime        = rt;
	printf("frame\n");
	fflush(stdout);
	xtk_rt_frame(rt);
	printf("layout\n");
	fflush(stdout);
	xent_layout(ctx, host, 800, 600);
	printf("ok\n");
	return 0;
}
