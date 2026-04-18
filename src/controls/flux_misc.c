#include "../flux_render_internal.h"

void flux_draw_container(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	flux_fill_background(rc, snap, bounds);
	flux_stroke_border(rc, snap, bounds);
}
