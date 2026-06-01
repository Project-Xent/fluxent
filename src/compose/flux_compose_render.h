/**
 * @file flux_compose_render.h
 * @brief Live retained-composition render path (ADR 0001 Phase B end-state).
 *
 * Ties the pieces together: reconciles the layout tree into a WUC visual tree,
 * then gives each node its own `SpriteVisual` + `CompositionDrawingSurface` and
 * paints it by **reusing the existing 19 Direct2D control renderers** through
 * `flux_engine_dispatch_render` — no renderer rewrite. The compositor composes
 * the per-node surfaces; the swap chain is unused in this mode.
 *
 * Driven behind a default-off flag (`FLUX_USE_COMPOSITION=1`); the classic
 * immediate-mode path is unchanged when the flag is off.
 *
 * NOTE: compile-verified end-to-end wiring. Pixel correctness, scroll clipping,
 * overlay (scrollbar) phase, and per-surface brush caching are the runtime
 * tuning steps on a GPU session.
 */
#ifndef FLUX_COMPOSE_RENDER_H
#define FLUX_COMPOSE_RENDER_H

#include "compose/flux_compose.h"
#include "fluxent/flux_node_store.h"

typedef struct FluxRenderContext FluxRenderContext;

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxComposeRender FluxComposeRender;

/**
 * @brief Create the composition render path.
 *
 * @param c    Shared compositor.
 * @param gdev Graphics device (per-node surfaces draw through it; must share
 *             the D2D device with the render context's `d2d`).
 * @param root The visual under which the mirrored tree is built.
 * @return New render path, or NULL on failure.
 */
FluxComposeRender *flux_compose_render_create(FluxCompositor *c, WUC_CompositionGraphicsDevice *gdev, WUC_Visual *root);

/** @brief Destroy the render path and all per-node visuals/surfaces. NULL safe. */
void               flux_compose_render_destroy(FluxComposeRender *r);

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
void               flux_compose_render_frame(
  FluxComposeRender *r, XentContext *ctx, FluxNodeStore *store, XentNodeId root, FluxRenderContext const *rc_tmpl
);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_RENDER_H */
