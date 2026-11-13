#include "app/flux_scene.h"
#include "store/flux_node_data.h"

#include <stdlib.h>

struct FluxScene {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId root;
	XentNodeId content;
	XentNodeId overlay;
	bool       owns_ctx;
	bool       owns_store;
};

static XentNodeId scene_make_layer(FluxScene *scene, XentProtocol proto) {
	XentNodeId n = xent_node_create(scene->ctx);
	if (n == XENT_NODE_INVALID) return n;
	xent_node_append(scene->ctx, scene->root, n);
	flux_node_settype(scene->store, n, FLUX_CONTROL_CONTAINER);
	xent_sem_setrole(scene->ctx, n, XENT_SEM_ROLE_CUSTOM);
	xent_setproto(scene->ctx, n, proto);
	xent_setpos(scene->ctx, n, (XentPoint) {0.0f, 0.0f});
	xent_setsizepct(scene->ctx, n, (XentSize) {1.0f, 1.0f});
	FluxNodeData *nd = flux_store_ensure(scene->store, n);
	if (nd) nd->component_type = FLUX_CONTROL_CONTAINER;
	return n;
}

FluxScene *flux_scene_create(uint32_t initial_capacity) {
	FluxScene *scene = ( FluxScene * ) calloc(1, sizeof(*scene));
	if (!scene) return NULL;

	XentCfg cfg          = {0};
	cfg.initial_capacity = initial_capacity;
	scene->ctx           = xent_ctx_create(&cfg);
	scene->store         = flux_store_create(initial_capacity);
	if (!scene->ctx || !scene->store) {
		flux_scene_destroy(scene);
		return NULL;
	}

	scene->root = xent_node_create(scene->ctx);
	if (scene->root == XENT_NODE_INVALID) {
		flux_scene_destroy(scene);
		return NULL;
	}

	if (!flux_store_bind(scene->store, scene->ctx)) {
		flux_scene_destroy(scene);
		return NULL;
	}

	flux_node_settype(scene->store, scene->root, FLUX_CONTROL_CONTAINER);
	xent_sem_setrole(scene->ctx, scene->root, XENT_SEM_ROLE_CUSTOM);

	xent_setproto(scene->ctx, scene->root, XENT_PROTOCOL_ABSOLUTE);

	FluxNodeData *nd = flux_store_ensure(scene->store, scene->root);
	if (nd) nd->component_type = FLUX_CONTROL_CONTAINER;

	scene->content = scene_make_layer(scene, XENT_PROTOCOL_FLEX);
	scene->overlay = scene_make_layer(scene, XENT_PROTOCOL_ABSOLUTE);
	if (scene->content == XENT_NODE_INVALID || scene->overlay == XENT_NODE_INVALID) {
		flux_scene_destroy(scene);
		return NULL;
	}
	xent_setflexdir(scene->ctx, scene->content, XENT_FLEX_COLUMN);
	xent_setitems(scene->ctx, scene->content, XENT_FLEX_ALIGN_STRETCH);

	scene->owns_ctx   = true;
	scene->owns_store = true;
	return scene;
}

FluxScene *flux_scene_borrow(XentCtx *ctx, FluxStore *store, XentNodeId root) {
	if (!ctx || !store || root == XENT_NODE_INVALID) return NULL;

	FluxScene *scene = ( FluxScene * ) calloc(1, sizeof(*scene));
	if (!scene) return NULL;

	scene->ctx     = ctx;
	scene->store   = store;
	scene->root    = root;
	scene->content = root;
	scene->overlay = root;
	if (!flux_store_bind(store, ctx)) {
		free(scene);
		return NULL;
	}
	return scene;
}

void flux_scene_destroy(FluxScene *scene) {
	if (!scene) return;
	if (scene->owns_store) flux_store_destroy(scene->store);
	if (scene->owns_ctx) xent_ctx_destroy(scene->ctx);
	free(scene);
}

XentCtx   *flux_scene_context(FluxScene const *scene) { return scene ? scene->ctx : NULL; }

FluxStore *flux_scene_store(FluxScene const *scene) { return scene ? scene->store : NULL; }

XentNodeId flux_scene_root(FluxScene const *scene) { return scene ? scene->root : XENT_NODE_INVALID; }

XentNodeId flux_scene_content(FluxScene const *scene) { return scene ? scene->content : XENT_NODE_INVALID; }

XentNodeId flux_scene_overlay(FluxScene const *scene) { return scene ? scene->overlay : XENT_NODE_INVALID; }
