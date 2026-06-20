/**
 * @file flux_backend.c
 * @brief XtkBackend implementation for fluxent (Windows/D3D11/D2D).
 */
#include "flux_internal.h"

#include <stdlib.h>

static void *flux_be_binding_create(void *ctx, XtkRuntime *rt) {
	( void ) ctx;
	FluxBinding *b = ( FluxBinding * ) calloc(1, sizeof(*b));
	if (b) b->rt = rt;
	return b;
}

static XentNodeId flux_be_create(void *ctx, XentNodeId parent, XtkEl const *el, void *binding) {
	return flux_create_control(( FluxBackendCtx * ) ctx, parent, el, ( FluxBinding * ) binding);
}

static void flux_be_destroy(void *ctx, XentNodeId id) {
	flux_subtree_destroy((( FluxBackendCtx * ) ctx)->store, id);
}

static void flux_be_pre_unmount(void *ctx, XentNodeId id) {
	FluxBackendCtx *rt = ( FluxBackendCtx * ) ctx;
	if (flux_get_control_type(rt->ctx, id) == FLUX_CONTROL_CONTENT_DIALOG)
		flux_content_dialog_hide(rt->store, id);
}

static void flux_be_mount_setup(void *ctx, XtkNode *node, XentNodeId id, XtkEl const *el) {
	flux_mount_setup(( FluxBackendCtx * ) ctx, node, id, el);
}

static void flux_be_apply_props(void *ctx, XtkNode *node, XtkEl const *prev, XtkEl const *el) {
	flux_apply_props(( FluxBackendCtx * ) ctx, node, prev, el);
}

static void flux_be_request_frame(void *ctx) {
	FluxBackendCtx *rt = ( FluxBackendCtx * ) ctx;
	if (rt->app) flux_app_request_render(rt->app);
}

static void flux_be_node_cleanup(XtkNode *n) {
	FluxNodeExt *ext = ( FluxNodeExt * ) n->ext;
	if (ext) {
		if (ext->menu) flux_menu_flyout_destroy(ext->menu);
		free(ext->menu_bindings);
		free(ext->tab_hosts);
		free(ext);
	}
	free(n->binding);
}

XtkBackend flux_xtk_backend(FluxBackendCtx *bctx) {
	return (XtkBackend) {
	  .ctx            = bctx,
	  .binding_create = flux_be_binding_create,
	  .create         = flux_be_create,
	  .destroy        = flux_be_destroy,
	  .pre_unmount    = flux_be_pre_unmount,
	  .mount_setup    = flux_be_mount_setup,
	  .apply_props    = flux_be_apply_props,
	  .is_interactive = flux_is_interactive,
	  .request_frame  = flux_be_request_frame,
	  .node_cleanup   = flux_be_node_cleanup,
	};
}
