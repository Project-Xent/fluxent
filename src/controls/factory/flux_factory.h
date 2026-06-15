/**
 * @file flux_factory.h
 * @brief Control factory functions for creating Fluxent UI controls.
 *
 * These functions create layout nodes with associated FluxNodeData,
 * register renderers, and wire up input callbacks.
 */
#ifndef FLUX_FACTORY_H
#define FLUX_FACTORY_H

#include "fluxent/flux_node_store.h"
#include "fluxent/flux_component_data.h"
#include "fluxent/flux_controls.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_menu_flyout.h"
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxWindow FluxWindow;
typedef struct FluxFlyout FluxFlyout;

/**
 * @brief Create a node and attach it to a parent.
 *
 * Sets control type, creates FluxNodeData, and attaches userdata.
 * Used internally by all flux_create_* functions.
 */
XentNodeId flux_factory_create_node(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, FluxControlType type);

/**
 * @brief WinUI-metric default sizing for leaf controls.
 *
 * Label-driven controls get a text-derived intrinsic size (label + chrome
 * padding) via xent's text measurement; controls without a label get the
 * fixed fallback box (NAN axes stay auto). An explicit xent_set_size from
 * the caller overrides these defaults.
 */
typedef struct FluxLeafMetrics {
	XentContext *ctx;
	XentNodeId   node;
	char const  *label;    /**< NULL or empty: use fallback instead. */
	XentInsets   padding;  /**< Chrome around the measured label. */
	XentSize     min_size; /**< Applied when either axis is > 0. */
	XentSize     fallback; /**< Size when there is no label (NAN = auto axis). */
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
FluxRect flux_binding_screen_anchor(FluxWindow *window, XentContext *ctx, FluxNodeStore *store, XentNodeId node);

/**
 * @brief Bind a flyout to a node's pointer-down event.
 */
void     flux_node_set_flyout(FluxNodeStore *store, XentNodeId id, FluxFlyout *flyout, FluxPlacement placement);

/**
 * @brief Bind a flyout with full context for anchor calculation.
 */
void     flux_node_set_flyout_ex(FluxFlyoutBindingInfo const *info);

/**
 * @brief Bind a context menu flyout to a node.
 */
void     flux_node_set_context_flyout(FluxNodeStore *store, XentNodeId id, FluxMenuFlyout *menu);

/**
 * @brief Bind a context menu flyout with full context.
 */
void     flux_node_set_context_flyout_ex(FluxContextFlyoutBindingInfo const *info);

#ifdef __cplusplus
}
#endif

#endif
