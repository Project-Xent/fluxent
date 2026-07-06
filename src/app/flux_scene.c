#include "app/flux_scene.h"

#include <stdlib.h>

struct FluxScene {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     root;
	XentNodeId     content;
	XentNodeId     overlay;
	bool           owns_ctx;
	bool           owns_store;
};

/* A full-window layer under the absolute root: absolute origin + 100% size. */
static XentNodeId scene_make_layer(FluxScene *scene, XentProtocol proto) {
	XentNodeId n = xent_create_node(scene->ctx);
	if (n == XENT_NODE_INVALID) return n;
	xent_append_child(scene->ctx, scene->root, n);
	flux_set_control_type(scene->ctx, n, FLUX_CONTROL_CONTAINER);
	xent_set_semantic_role(scene->ctx, n, XENT_SEMANTIC_CUSTOM);
	xent_set_protocol(scene->ctx, n, proto);
	xent_set_absolute_position(scene->ctx, n, (XentPoint) {0.0f, 0.0f});
	xent_set_size_percent(scene->ctx, n, (XentSize) {1.0f, 1.0f});
	FluxNodeData *nd = flux_node_store_get_or_create(scene->store, n);
	if (nd) nd->component_type = FLUX_CONTROL_CONTAINER;
	xent_set_userdata(scene->ctx, n, nd);
	return n;
}

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

	flux_node_store_bind_context(scene->store, scene->ctx);

	flux_set_control_type(scene->ctx, scene->root, FLUX_CONTROL_CONTAINER);
	xent_set_semantic_role(scene->ctx, scene->root, XENT_SEMANTIC_CUSTOM);

	/* Root is an absolute stack: content layer (reconciled app view) with an overlay
	 * layer above it for modal surfaces (dialogs, flyouts) so they overlay the page. */
	xent_set_protocol(scene->ctx, scene->root, XENT_PROTOCOL_ABSOLUTE);

	FluxNodeData *nd = flux_node_store_get_or_create(scene->store, scene->root);
	if (nd) nd->component_type = FLUX_CONTROL_CONTAINER;
	xent_set_userdata(scene->ctx, scene->root, nd);

	scene->content = scene_make_layer(scene, XENT_PROTOCOL_FLEX);
	scene->overlay = scene_make_layer(scene, XENT_PROTOCOL_ABSOLUTE);
	if (scene->content == XENT_NODE_INVALID || scene->overlay == XENT_NODE_INVALID) {
		flux_scene_destroy(scene);
		return NULL;
	}
	/* Content keeps the old root's column-stretch (top-level column / stretched band
	 * like a TitleBar spans the window). */
	xent_set_flex_direction(scene->ctx, scene->content, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(scene->ctx, scene->content, XENT_FLEX_ALIGN_STRETCH);

	scene->owns_ctx   = true;
	scene->owns_store = true;
	return scene;
}

FluxScene *flux_scene_borrow(XentContext *ctx, FluxNodeStore *store, XentNodeId root) {
	if (!ctx || !store || root == XENT_NODE_INVALID) return NULL;

	FluxScene *scene = ( FluxScene * ) calloc(1, sizeof(*scene));
	if (!scene) return NULL;

	scene->ctx     = ctx;
	scene->store   = store;
	scene->root    = root;
	scene->content = root; /* borrow mode: no separate overlay layer */
	scene->overlay = root;
	flux_node_store_bind_context(store, ctx);
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

XentNodeId     flux_scene_content(FluxScene const *scene) { return scene ? scene->content : XENT_NODE_INVALID; }

XentNodeId     flux_scene_overlay(FluxScene const *scene) { return scene ? scene->overlay : XENT_NODE_INVALID; }
