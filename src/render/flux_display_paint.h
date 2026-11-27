#ifndef FLUX_DISPLAY_PAINT_H
#define FLUX_DISPLAY_PAINT_H

#include "render/flux_engine.h"
#include "render/flux_render_cache.h"
#include "fluxent/flux_node_store.h"

typedef struct XtkBackend XtkBackend;

/** Whether the existing display list for `nd` can be replayed without calling record()
 *  again. A live state tween forces a re-record: the tween only advances inside
 *  record(), and the snapshot hash stays put while it runs. */
bool                      flux_display_paint_can_reuse(
  FluxNodeData const *nd, uint64_t hash, float w, float h, bool force_dyn, FluxCacheEntry const *entry
);

void flux_display_paint_main(
  FluxStore *store, XtkBackend *backend, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);

void flux_display_paint_overlay(
  XtkBackend *backend, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);

#endif
