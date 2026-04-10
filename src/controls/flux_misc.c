#include "../flux_render_internal.h"

void flux_draw_container(const FluxRenderContext *rc,
                         const FluxRenderSnapshot *snap,
                         const FluxRect *bounds,
                         const FluxControlState *state) {
    (void)state;
    flux_fill_background(rc, snap, bounds);
    flux_stroke_border(rc, snap, bounds);
}