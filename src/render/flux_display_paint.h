#ifndef FLUX_DISPLAY_PAINT_H
#define FLUX_DISPLAY_PAINT_H

#include "render/flux_engine.h"
#include "fluxent/flux_node_store.h"

typedef struct XtkBackend XtkBackend;

void                      flux_display_paint_main(
  FluxStore *store, XtkBackend *backend, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);

void flux_display_paint_overlay(
  XtkBackend *backend, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);

#endif
