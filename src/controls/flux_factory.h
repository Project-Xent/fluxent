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
#include "fluxent/flux_popup.h"
#include "fluxent/flux_menu_flyout.h"
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Forward declarations */
typedef struct FluxWindow FluxWindow;
typedef struct FluxFlyout FluxFlyout;

/* ═══════════════════════════════════════════════════════════════════════
   Internal helper
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a node and attach it to a parent.
 *
 * Sets control type, creates FluxNodeData, and attaches userdata.
 * Used internally by all flux_create_* functions.
 */
XentNodeId flux_factory_create_node(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, XentControlType type);

/* ═══════════════════════════════════════════════════════════════════════
   Flyout binding helpers
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Bind a flyout to a node's pointer-down event.
 */
void       flux_node_set_flyout(FluxNodeStore *store, XentNodeId id, FluxFlyout *flyout, FluxPlacement placement);

/**
 * @brief Bind a flyout with full context for anchor calculation.
 */
void       flux_node_set_flyout_ex(
  FluxNodeStore *store, XentNodeId id, FluxFlyout *flyout, FluxPlacement placement, XentContext *xctx,
  FluxWindow *window
);

/**
 * @brief Bind a context menu flyout to a node.
 */
void flux_node_set_context_flyout(FluxNodeStore *store, XentNodeId id, FluxMenuFlyout *menu);

/**
 * @brief Bind a context menu flyout with full context.
 */
void flux_node_set_context_flyout_ex(
  FluxNodeStore *store, XentNodeId id, FluxMenuFlyout *menu, XentContext *xctx, FluxWindow *window
);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_FACTORY_H */
