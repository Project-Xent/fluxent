/**
 * @file flux_visual_tree.h
 * @brief Identity-stable reconciler: layout tree -> WUC visual tree.
 *
 * The reconciler is the heart of the retained-composition architecture (ADR
 * 0001 §3.2). It maintains a persistent tree of `ContainerVisual`s mirroring the
 * Xent layout nodes, keyed by `XentNodeId` so a node keeps the *same* visual
 * across frames — which is mandatory, because the compositor's running
 * animations live on those visuals and must not be reset.
 *
 * Each reconcile pass:
 *   1. bumps a generation counter,
 *   2. walks the layout subtree, creating a container per node on first sight
 *      and updating offset/size every time,
 *   3. mark-and-sweeps: any visual whose node was not seen this pass is detached
 *      and released.
 *
 * It owns *topology only* (container visuals + parent/child + offset/size/clip).
 * Per-control payload (shapes, text surfaces, brushes) is attached by control
 * builders in a later phase — keeping topology and payload separate per
 * Doctrine #2.
 *
 * NOTE: this module is built and compile-verified but not yet wired into the
 * live frame loop; that swap (and the per-control payload builders) lands with
 * GPU runtime validation per the ADR's phased migration.
 */
#ifndef FLUX_COMPOSE_VISUAL_TREE_H
#define FLUX_COMPOSE_VISUAL_TREE_H

#include "compose/flux_compose.h"
#include "fluxent/flux_node_store.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxVisualTree FluxVisualTree;

/**
 * @brief Create a reconciler attached under a parent visual.
 *
 * @param c           Shared compositor.
 * @param root_parent The visual under which the mirrored tree is built
 *                    (typically the window target's root). Must be a container
 *                    visual or sprite visual (both expose a Children collection).
 * @return New reconciler, or NULL on failure.
 */
FluxVisualTree               *flux_visual_tree_create(FluxCompositor *c, WUC_Visual *root_parent);

/**
 * @brief Destroy the reconciler and release every visual it created. NULL safe.
 */
void                          flux_visual_tree_destroy(FluxVisualTree *vt);

/**
 * @brief Reconcile the visual tree against the current layout under `root`.
 *
 * Creates/updates/removes container visuals so the tree matches the layout.
 * Cheap when nothing structural changed; only touches visuals whose nodes are
 * present, and removes those whose nodes vanished.
 *
 * Layout is in DIPs; the compositor works in physical pixels. `scale`
 * (dpi/96) converts node offsets and sizes to physical pixels so the mirrored
 * tree matches the display, exactly as D2D's device-context DPI does for the
 * classic path.
 *
 * Scroll nodes are clipped to their viewport and their content is shifted by the
 * current scroll offset (read from the node store), so scrolling works without a
 * command list -- the retained-tree equivalent of the classic path's clip +
 * translate.
 *
 * @param vt    Reconciler.
 * @param ctx   Xent layout context (must be laid out already).
 * @param store Node store, for per-node component data (scroll offsets); may be NULL.
 * @param root  Subtree root to reconcile.
 * @param scale DIP-to-pixel factor (dpi/96); values <= 0 are treated as 1.
 */
void flux_visual_tree_reconcile(
  FluxVisualTree *vt, XentContext *ctx, FluxNodeStore *store, XentNodeId root, float scale
);

/**
 * @brief The container visual mirroring a layout node, or NULL if absent.
 *
 * Control payload builders attach shapes/surfaces to this container.
 */
WUC_Container                *flux_visual_tree_node_visual(FluxVisualTree *vt, XentNodeId node);

/**
 * @brief The scroll content holder visual for a scroll node, or NULL.
 *
 * A scroll node routes its children through this holder; binding the holder's
 * Offset to an InteractionTracker's -Position scrolls the content on the
 * compositor thread. Returns NULL for non-scroll nodes or unknown ids.
 */
WUC_Visual                   *flux_visual_tree_node_scroll_holder(FluxVisualTree *vt, XentNodeId node);

/**
 * @brief Mark whether a scroll node's holder Offset is tracker-driven.
 *
 * When set, the reconciler stops folding the scroll offset into child positions
 * (the tracker-bound holder owns the offset instead). NULL/unknown id is a no-op.
 */
void                          flux_visual_tree_set_tracker_bound(FluxVisualTree *vt, XentNodeId node, bool bound);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_VISUAL_TREE_H */
