#ifndef FLUX_FLYOUT_BINDING_H
#define FLUX_FLYOUT_BINDING_H

#include "fluxent/flux_node_store.h"
#include "fluxent/flux_popup.h"

typedef struct FluxFlyout FluxFlyout;
typedef struct FluxMenu   FluxMenu;
typedef struct FluxWindow FluxWindow;

typedef struct FluxFlyoutBindingInfo {
	FluxStore    *store;
	XentNodeId    id;
	FluxFlyout   *flyout;
	FluxPlacement placement;
	XentCtx      *xctx;
	FluxWindow   *window;
} FluxFlyoutBindingInfo;

typedef struct FluxContextFlyoutBindingInfo {
	FluxStore  *store;
	XentNodeId  id;
	FluxMenu   *menu;
	XentCtx    *xctx;
	FluxWindow *window;
} FluxContextFlyoutBindingInfo;

void     flux_split_retarget(FluxStore *store, XentNodeId id);

FluxRect flux_binding_window_rect(XentCtx *ctx, FluxStore *store, XentNodeId node);

void     flux_prop_split_menu_ex(FluxContextFlyoutBindingInfo const *info);
void     flux_split_clear_menu(FluxStore *store, XentNodeId id, FluxMenu *menu);

void     flux_prop_menu_ex(FluxContextFlyoutBindingInfo const *info);

#endif
