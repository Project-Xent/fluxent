/**
 * @file flux_factory.h
 * @brief Control factory functions for creating Fluxent UI controls.
 *
 * These functions create layout nodes with associated FluxNodeData and wire
 * up input callbacks.
 */
#ifndef FLUX_FACTORY_H
#define FLUX_FACTORY_H

#include "fluxent/flux_node_store.h"
#include "store/flux_node_data.h"
#include "controls/data/flux_component_data.h"
#include "host/flux_prop_apply.h"
#include "controls/behavior/flux_flyout_binding.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_menu.h"
#include <xent/xent.h>


typedef struct FluxWindow FluxWindow;
typedef struct FluxFlyout FluxFlyout;

/**
 * @brief Create a node and attach it to a parent.
 *
 * Sets the control type and creates FluxNodeData.
 * Used internally by all flux_<ctrl>_init functions.
 */
XentNodeId flux_factory_create_node(XentCtx *ctx, FluxStore *store, XentNodeId parent, FluxControlType type);

/**
 * @brief Roll back a node whose component-data allocation failed.
 *
 * Removes the node's store entry, destroys the xent node, and returns
 * XENT_NODE_INVALID — so a create path never publishes a half-built node on OOM.
 * free() the (possibly NULL) component-data allocation before calling.
 */
XentNodeId flux_factory_fail_node(XentCtx *ctx, FluxStore *store, XentNodeId node);

/**
 * @brief WinUI-metric default sizing for leaf controls.
 *
 * Label-driven controls get a text-derived intrinsic size (label + chrome
 * padding) via xent's text measurement; controls without a label get the
 * fixed fallback box (NAN axes stay auto). An explicit xent_setsize from
 * the caller overrides these defaults.
 */
typedef struct FluxLeafMetrics {
	XentCtx    *ctx;
	XentNodeId  node;
	char const *label;    /**< NULL or empty: use fallback instead. */
	XentInsets  padding;  /**< Chrome around the measured label. */
	XentSize    min_size; /**< Applied when either axis is > 0. */
	XentSize    fallback; /**< Size when there is no label (NAN = auto axis). */
} FluxLeafMetrics;

void     flux_leaf_default_metrics(FluxLeafMetrics const *m);

/**
 * @brief Destructor for an owned FluxButtonData: frees the label/icon copies
 * and the struct. Shared by button, dropdown button, and split button.
 */
void     flux_button_data_destroy(void *component_data);

/**
 * @brief Resolve a node's on-screen anchor rect (DPI-scaled, scroll-adjusted).
 * Falls back to the cursor position when window/ctx/node are unavailable.
 * Used by flyout/split-button bindings to position popups under a control.
 */
FluxRect flux_binding_screen_anchor(FluxWindow *window, XentCtx *ctx, FluxStore *store, XentNodeId node);

/**
 * @brief Bind a flyout to a node's pointer-down event.
 */
void     flux_prop_node_flyout(FluxStore *store, XentNodeId id, FluxFlyout *flyout, FluxPlacement placement);

/**
 * @brief Bind a flyout with full context for anchor calculation.
 */
void     flux_prop_flyout_ex(FluxFlyoutBindingInfo const *info);

/**
 * @brief Bind a context menu flyout to a node.
 */
void     flux_prop_ctxflyout(FluxStore *store, XentNodeId id, FluxMenu *menu);

/**
 * @brief Bind a context menu flyout with full context.
 */
void     flux_prop_ctxflyout_ex(FluxContextFlyoutBindingInfo const *info);


#endif
