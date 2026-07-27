/**
 * @file flux_compose_render.h
 * @brief Live retained-composition render path.
 *
 * Reconciles the layout tree into a WUC visual tree, then paints each node into
 * its own surface via `flux_display_paint_main` (record/replay).
 */
#ifndef FLUX_COMPOSE_RENDER_H
#define FLUX_COMPOSE_RENDER_H

#include "compose/flux_compose.h"
#include "fluxent/flux_node_store.h"

typedef struct FluxRenderCtx       FluxRenderCtx;
typedef struct FluxControlRegistry FluxControlRegistry;
typedef struct XtkBackend          XtkBackend;


typedef struct FluxComposeRender   FluxComposeRender;

/**
 * @brief Create the composition render path.
 *
 * @param c        Shared compositor.
 * @param gdev     Graphics device (per-node surfaces draw through it; must share
 *                 the D2D device with the render context's `d2d`).
 * @param root     The visual under which the mirrored tree is built.
 * @param registry Private control record/draw table.
 * @param backend  XtkBackend used to replay display lists (may be NULL).
 * @return New render path, or NULL on failure.
 */
FluxComposeRender                 *flux_compose_render_create(
  FluxCompositor *c, WUC_CompositionGraphicsDevice *gdev, WUC_Visual *root, FluxControlRegistry const *registry,
  XtkBackend *backend
);

/** @brief Destroy the render path and all per-node visuals/surfaces. NULL safe. */
void flux_compose_render_destroy(FluxComposeRender *r);

/**
 * @brief Reconcile and paint one frame.
 *
 * @param r        Render path.
 * @param ctx      Laid-out Xent context.
 * @param store    Node store (for dispatch).
 * @param root     Subtree root.
 * @param rc_tmpl  Render-context template (text/cache/theme/dpi/timing); the
 *                 per-surface `d2d` and `brush` are filled in per node.
 */
void flux_compose_render_frame(
  FluxComposeRender *r, XentCtx *ctx, FluxStore *store, XentNodeId root, FluxRenderCtx const *rc_tmpl
);

/**
 * @brief Hand pointer @p pointer_id to scroll node @p node's InteractionTracker
 *        (touch/touchpad pan handoff, the composition counterpart of the DManip
 *        handoff). Returns true if the pointer was redirected.
 */
bool flux_compose_redirect_scroll(FluxComposeRender *r, XentNodeId node, uint32_t pointer_id);


#endif /* FLUX_COMPOSE_RENDER_H */
