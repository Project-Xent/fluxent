#include "app/flux_scene.h"

#include <stdlib.h>

struct FluxScene {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     root;
	bool           owns_ctx;
	bool           owns_store;
};

FluxScene *flux_scene_create(uint32_t initial_capacity) {
	FluxScene *scene = ( FluxScene * ) calloc(1, sizeof(*scene));
	if (!scene) return NULL;

	XentConfig cfg       = {0};
	cfg.initial_capacity = initial_capacity;
	scene->ctx           = xent_create_context(&cfg);
	scene->store         = flux_node_store_create(initial_capacity);
	if (!scene->ctx || !scene->store) {
		flux_scene_destroy(scene);
		return NULL;
	}

	scene->root = xent_create_node(scene->ctx);
	if (scene->root == XENT_NODE_INVALID) {
		flux_scene_destroy(scene);
		return NULL;
	}

	flux_set_control_type(scene->ctx, scene->root, FLUX_CONTROL_CONTAINER);
	xent_set_semantic_role(scene->ctx, scene->root, XENT_SEMANTIC_CUSTOM);

	FluxNodeData *nd = flux_node_store_get_or_create(scene->store, scene->root);
	if (nd) nd->component_type = FLUX_CONTROL_CONTAINER;
	xent_set_userdata(scene->ctx, scene->root, nd);

	scene->owns_ctx   = true;
	scene->owns_store = true;
	return scene;
}

FluxScene *flux_scene_borrow(XentContext *ctx, FluxNodeStore *store, XentNodeId root) {
	if (!ctx || !store || root == XENT_NODE_INVALID) return NULL;

	FluxScene *scene = ( FluxScene * ) calloc(1, sizeof(*scene));
	if (!scene) return NULL;

	scene->ctx   = ctx;
	scene->store = store;
	scene->root  = root;
	return scene;
}

void flux_scene_destroy(FluxScene *scene) {
	if (!scene) return;
	if (scene->owns_store) flux_node_store_destroy(scene->store);
	if (scene->owns_ctx) xent_destroy_context(scene->ctx);
	free(scene);
}

XentContext   *flux_scene_context(FluxScene const *scene) { return scene ? scene->ctx : NULL; }

FluxNodeStore *flux_scene_store(FluxScene const *scene) { return scene ? scene->store : NULL; }

XentNodeId     flux_scene_root(FluxScene const *scene) { return scene ? scene->root : XENT_NODE_INVALID; }
