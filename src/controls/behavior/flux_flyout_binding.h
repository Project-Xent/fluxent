/**
 * @file flux_flyout_binding.h
 * @brief Private flyout / menu-flyout node binding helpers.
 */
#ifndef FLUX_FLYOUT_BINDING_H
#define FLUX_FLYOUT_BINDING_H

#include "fluxent/flux_node_store.h"
#include "fluxent/flux_popup.h"

typedef struct FluxFlyout FluxFlyout;
typedef struct FluxMenu   FluxMenu;
typedef struct FluxWindow FluxWindow;

/** @brief Full flyout binding parameters. */
typedef struct FluxFlyoutBindingInfo {
	FluxStore    *store;
	XentNodeId    id;
	FluxFlyout   *flyout;
	FluxPlacement placement;
	XentCtx      *xctx;
	FluxWindow   *window;
} FluxFlyoutBindingInfo;

/** @brief Full context-menu flyout binding parameters. */
typedef struct FluxContextFlyoutBindingInfo {
	FluxStore  *store;
	XentNodeId  id;
	FluxMenu   *menu;
	XentCtx    *xctx;
	FluxWindow *window;
} FluxContextFlyoutBindingInfo;

void flux_split_retarget(FluxStore *store, XentNodeId id);

void flux_prop_node_flyout(FluxStore *store, XentNodeId id, FluxFlyout *flyout, FluxPlacement placement);

void flux_prop_flyout_ex(FluxFlyoutBindingInfo const *info);

void flux_prop_split_flyout_ex(FluxFlyoutBindingInfo const *info);

void flux_prop_split_menu_ex(FluxContextFlyoutBindingInfo const *info);
void flux_split_clear_menu(FluxStore *store, XentNodeId id, FluxMenu *menu);

/** @brief Attach a context menu shown on right-click / long-press. */
void flux_prop_ctxflyout(FluxStore *store, XentNodeId id, FluxMenu *menu);

/** @brief Attach a context menu with explicit context references. */
void flux_prop_ctxflyout_ex(FluxContextFlyoutBindingInfo const *info);

void flux_prop_menu(FluxStore *store, XentNodeId id, FluxMenu *menu);

/** @brief Attach a primary-invoke MenuFlyout with explicit context references. */
void flux_prop_menu_ex(FluxContextFlyoutBindingInfo const *info);

#endif
