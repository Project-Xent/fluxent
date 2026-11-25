#ifndef FLUX_POPUP_TREE_H
#define FLUX_POPUP_TREE_H

#include <fluxent/flux_popup.h>
#include "render/flux_render_internal.h"

typedef struct FluxPopupTree  FluxPopupTree;
typedef struct FluxUiaContext FluxUiaContext;
typedef LRESULT               (*FluxPopupObjectFn)(void *ctx, HWND hwnd, WPARAM wp, LPARAM lp);
typedef bool                  (*FluxPopupOwnerPressFn)(void *ctx, XentNodeId node);

FluxPopupTree *flux_popup_tree_create(FluxWindow *owner, XentCtx *ctx, FluxStore *store, XentNodeId root);
void           flux_popup_tree_destroy(FluxPopupTree *tree);
bool           flux_popup_tree_paint(FluxPopupTree *tree, FluxPopup *popup, FluxRenderCtx const *surface);
bool           flux_popup_commandbar_access_scope(HWND owner, FluxUiaContext *info);
bool           flux_popup_sethost(FluxPopup *popup, FluxInput *input, XentNodeId owner);
void           flux_popup_rebind(FluxPopup *popup, XentNodeId owner);
void           flux_popup_onownerpress(FluxPopup *popup, FluxPopupOwnerPressFn cb, void *ctx);
bool           flux_popup_ownerpress_passes(HWND owner, XentNodeId node);
float          flux_popup_fit_content_height(FluxPopup const *popup, float height);
void           flux_popup_setscale(FluxPopup *popup, float sx, float sy, float cx, float cy);
void           flux_popup_onobject(FluxPopup *popup, FluxPopupObjectFn cb, void *ctx);

#endif
