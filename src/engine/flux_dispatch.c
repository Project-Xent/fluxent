#include "fluxent/flux_engine.h"
#include <string.h>

static FluxControlRenderer g_renderers [XENT_CONTROL_CUSTOM + 1];

void                       flux_register_renderer(
  XentControlType type,
  void (*draw)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *),
  void (*draw_overlay)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *)
) {
		if (( uint32_t ) type <= XENT_CONTROL_CUSTOM) {
			g_renderers [type].draw         = draw;
			g_renderers [type].draw_overlay = draw_overlay;
		}
}

void flux_dispatch_render(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	uint32_t idx = ( uint32_t ) snap->type;
	if (idx <= XENT_CONTROL_CUSTOM && g_renderers [idx].draw) g_renderers [idx].draw(rc, snap, bounds, state);
}

void flux_dispatch_render_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds) {
	uint32_t idx = ( uint32_t ) snap->type;
	if (idx <= XENT_CONTROL_CUSTOM && g_renderers [idx].draw_overlay) g_renderers [idx].draw_overlay(rc, snap, bounds);
}
