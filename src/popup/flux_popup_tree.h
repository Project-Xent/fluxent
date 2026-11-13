#ifndef FLUX_POPUP_TREE_H
#define FLUX_POPUP_TREE_H

#include <fluxent/flux_popup.h>
#include "render/flux_render_internal.h"

typedef struct FluxPopupTree FluxPopupTree;

FluxPopupTree               *flux_popup_tree_create(FluxWindow *owner, XentCtx *ctx, FluxStore *store, XentNodeId root);
void                         flux_popup_tree_destroy(FluxPopupTree *tree);
bool                         flux_popup_tree_paint(FluxPopupTree *tree, FluxPopup *popup, FluxRenderCtx const *surface);

#endif
