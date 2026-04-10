#ifndef FLUX_ENGINE_H
#define FLUX_ENGINE_H

#include "flux_render_snapshot.h"
#include "flux_node_store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxRenderContext FluxRenderContext;
typedef struct FluxEngine FluxEngine;

typedef struct FluxControlState {
    uint8_t hovered;
    uint8_t pressed;
    uint8_t focused;
    uint8_t enabled;
} FluxControlState;

typedef enum FluxCommandPhase {
    FLUX_PHASE_MAIN,
    FLUX_PHASE_OVERLAY,
} FluxCommandPhase;

typedef enum FluxClipAction {
    FLUX_CLIP_NONE,
    FLUX_CLIP_PUSH,
    FLUX_CLIP_POP,
} FluxClipAction;

typedef struct FluxRenderCommand {
    FluxRenderSnapshot snapshot;
    FluxRect           bounds;
    FluxControlState   state;
    FluxCommandPhase   phase;
    FluxClipAction     clip_action;
    float              scroll_x;
    float              scroll_y;
} FluxRenderCommand;

typedef struct FluxControlRenderer {
    void (*draw)(const FluxRenderContext *rc,
                 const FluxRenderSnapshot *snap,
                 const FluxRect *bounds,
                 const FluxControlState *state);
    void (*draw_overlay)(const FluxRenderContext *rc,
                         const FluxRenderSnapshot *snap,
                         const FluxRect *bounds);
} FluxControlRenderer;

FluxEngine *flux_engine_create(FluxNodeStore *store);
void        flux_engine_destroy(FluxEngine *eng);

void flux_engine_collect(FluxEngine *eng, XentContext *ctx, XentNodeId root);

uint32_t              flux_engine_command_count(const FluxEngine *eng);
const FluxRenderCommand *flux_engine_command_at(const FluxEngine *eng, uint32_t index);

void flux_engine_execute(const FluxEngine *eng, const FluxRenderContext *rc);

void flux_dispatch_render(const FluxRenderContext *rc,
                          const FluxRenderSnapshot *snap,
                          const FluxRect *bounds,
                          const FluxControlState *state);

void flux_dispatch_render_overlay(const FluxRenderContext *rc,
                                  const FluxRenderSnapshot *snap,
                                  const FluxRect *bounds);

#ifdef __cplusplus
}
#endif

#endif