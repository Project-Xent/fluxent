#include "fluxent/flux_engine.h"
#include <string.h>

static FluxControlRenderer g_renderers[XENT_CONTROL_CUSTOM + 1];

void flux_register_renderer(XentControlType type,
                            void (*draw)(const FluxRenderContext *,
                                         const FluxRenderSnapshot *,
                                         const FluxRect *,
                                         const FluxControlState *),
                            void (*draw_overlay)(const FluxRenderContext *,
                                                 const FluxRenderSnapshot *,
                                                 const FluxRect *)) {
    if ((uint32_t)type <= XENT_CONTROL_CUSTOM) {
        g_renderers[type].draw = draw;
        g_renderers[type].draw_overlay = draw_overlay;
    }
}

void flux_dispatch_render(const FluxRenderContext *rc,
                          const FluxRenderSnapshot *snap,
                          const FluxRect *bounds,
                          const FluxControlState *state) {
    uint32_t idx = (uint32_t)snap->type;
    if (idx <= XENT_CONTROL_CUSTOM && g_renderers[idx].draw) {
        g_renderers[idx].draw(rc, snap, bounds, state);
    }
}

void flux_dispatch_render_overlay(const FluxRenderContext *rc,
                                  const FluxRenderSnapshot *snap,
                                  const FluxRect *bounds) {
    uint32_t idx = (uint32_t)snap->type;
    if (idx <= XENT_CONTROL_CUSTOM && g_renderers[idx].draw_overlay) {
        g_renderers[idx].draw_overlay(rc, snap, bounds);
    }
}