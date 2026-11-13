#ifndef FLUX_COMPOSE_VISUAL_TREE_H
#define FLUX_COMPOSE_VISUAL_TREE_H

#include "compose/flux_compose.h"
#include "fluxent/flux_node_store.h"

typedef struct FluxVisualTree FluxVisualTree;

FluxVisualTree               *flux_visual_tree_create(FluxCompositor *c, WUC_Visual *root_parent);

void                          flux_visual_tree_destroy(FluxVisualTree *vt);

void flux_visual_tree_reconcile(FluxVisualTree *vt, XentCtx *ctx, FluxStore *store, XentNodeId root, float scale);

WUC_Container *flux_vtree_visual(FluxVisualTree *vt, XentNodeId node);

WUC_Visual    *flux_vtree_scrollholder(FluxVisualTree *vt, XentNodeId node);

void           flux_vtree_bindtracker(FluxVisualTree *vt, XentNodeId node, bool bound);

#endif
