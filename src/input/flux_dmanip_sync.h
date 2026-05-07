/**
 * @file flux_dmanip_sync.h
 * @brief DirectManipulation viewport synchronization for ScrollView nodes.
 *
 * Provides tree-walking functions to lazily create and sync DManip viewports
 * with the current layout, and to clean them up on teardown.
 */
#ifndef FLUX_DMANIP_SYNC_H
#define FLUX_DMANIP_SYNC_H

#include "fluxent/flux_node_store.h"
#include "flux_dmanip.h"
#include <xent/xent.h>

typedef struct FluxInput FluxInput;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Sync DManip viewports for all SCROLL nodes in the tree.
 *
 * Walks the layout tree from the given root, and for each SCROLL node:
 * - Lazily creates a DManip viewport if not yet present
 * - Updates viewport rect and content size from current layout
 * - Syncs the DManip transform if the node was scrolled externally
 *
 * Call once per frame after xent_layout() and flux_dmanip_tick().
 *
 * @param dmanip DManip manager (NULL = no-op).
 * @param ctx Xent layout context.
 * @param store FluxNodeStore for control metadata.
 * @param root Root node to walk.
 * @param scale DPI scale (pixels per DIP).
 */
void flux_dmanip_sync_tree(FluxDManip *dmanip, XentContext *ctx, FluxNodeStore *store, XentNodeId root, float scale);

/**
 * @brief Transfer the newly promoted touch pan from FluxInput to the DManip viewport for its scroll target.
 *
 * @param dmanip DManip manager.
 * @param input Input router tracking the promoted touch pan.
 * @param store FluxNodeStore for control metadata.
 * @param pointer_id WM_POINTER contact identifier.
 * @return true if DManip accepted the contact and FluxInput's manual pan state was cleared.
 */
bool flux_dmanip_handoff_touch_pan(FluxDManip *dmanip, FluxInput *input, FluxNodeStore *store, uint32_t pointer_id);

/**
 * @brief Destroy all DManip viewports attached to SCROLL nodes.
 *
 * Must be called before destroying the FluxDManip manager.
 *
 * @param ctx Xent layout context.
 * @param store FluxNodeStore for control metadata.
 * @param root Root node to walk.
 */
void flux_dmanip_cleanup_tree(XentContext *ctx, FluxNodeStore *store, XentNodeId root);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_DMANIP_SYNC_H */
