#ifndef FLUX_SCENE_H
#define FLUX_SCENE_H

#include "fluxent/flux_node_store.h"

typedef struct FluxScene FluxScene;

FluxScene               *flux_scene_create(uint32_t initial_capacity);
FluxScene               *flux_scene_borrow(XentContext *ctx, FluxNodeStore *store, XentNodeId root);
void                     flux_scene_destroy(FluxScene *scene);

XentContext             *flux_scene_context(FluxScene const *scene);
FluxNodeStore           *flux_scene_store(FluxScene const *scene);
XentNodeId               flux_scene_root(FluxScene const *scene);
XentNodeId               flux_scene_content(FluxScene const *scene);
XentNodeId               flux_scene_overlay(FluxScene const *scene);

#endif
