#include "fluxent/flux_engine.h"

void flux_dispatch_render(
  FluxNodeStore const *store, FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxControlRenderer const *renderer = flux_node_store_get_renderer(store, snap->type);
	if (renderer && renderer->draw) renderer->draw(rc, snap, bounds, state);
}

void flux_dispatch_render_overlay(
  FluxNodeStore const *store, FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds
) {
	FluxControlRenderer const *renderer = flux_node_store_get_renderer(store, snap->type);
	if (renderer && renderer->draw_overlay) renderer->draw_overlay(rc, snap, bounds);
}
