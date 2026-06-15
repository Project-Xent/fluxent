#include "fluxent/flux_engine.h"

void flux_engine_dispatch_render(
  FluxControlRegistry const *registry, FluxRenderContext const *rc, FluxRenderSnapshot const *snap,
  FluxRect const *bounds, FluxControlState const *state
) {
	FluxControlRenderer const *renderer = flux_control_registry_get(registry, snap->type);
	if (renderer && renderer->draw) renderer->draw(rc, snap, bounds, state);
}

void flux_engine_dispatch_render_overlay(
  FluxControlRegistry const *registry, FluxRenderContext const *rc, FluxRenderSnapshot const *snap,
  FluxRect const *bounds
) {
	FluxControlRenderer const *renderer = flux_control_registry_get(registry, snap->type);
	if (renderer && renderer->draw_overlay) renderer->draw_overlay(rc, snap, bounds);
}
