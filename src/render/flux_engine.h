#ifndef FLUX_ENGINE_H
#define FLUX_ENGINE_H

#include "flux_render_snapshot.h"
#include <fluxent/flux_node_store.h>

typedef struct FluxRenderCtx FluxRenderCtx;
typedef struct FluxEngine    FluxEngine;
typedef struct XtkBackend    XtkBackend;

typedef struct FluxControlState {
	uint8_t hovered;
	uint8_t pressed;
	uint8_t focused;
	uint8_t enabled;
	uint8_t pointer_type;
} FluxControlState;

typedef enum FluxCommandPhase
{
	FLUX_PHASE_MAIN,
	FLUX_PHASE_OVERLAY,
} FluxCommandPhase;

typedef enum FluxClipAction
{
	FLUX_CLIP_NONE,
	FLUX_CLIP_PUSH,
	FLUX_CLIP_POP,
	FLUX_CLIP_PUSH_TRANSFORM,
	FLUX_CLIP_POP_TRANSFORM,
} FluxClipAction;

typedef struct FluxRenderCommand {
	FluxRenderSnap   snapshot;
	FluxRect         bounds;
	FluxControlState state;
	FluxCommandPhase phase;
	FluxClipAction   clip_action;
	float            scroll_x;
	float            scroll_y;
	float            scale;
	float            pivot_x;
	float            pivot_y;
	float            opacity;
	float            translate_x;
	float            translate_y;
	bool             clip_subtree;
} FluxRenderCommand;

typedef void               (*FluxDegradedFn)(void *ctx);

XENT_NODISCARD FluxEngine *flux_engine_create(FluxStore *store);

void                       flux_engine_destroy(FluxEngine *eng);

void                       flux_engine_ondegraded(FluxEngine *eng, FluxDegradedFn cb, void *ctx);

void                       flux_engine_collect(FluxEngine *eng, XentCtx *ctx, XentNodeId root);

uint32_t                   flux_engine_ncmd(FluxEngine const *eng);

FluxRenderCommand const   *flux_engine_cmd(FluxEngine const *eng, uint32_t index);

void                       flux_engine_execute(FluxEngine const *eng, FluxRenderCtx const *rc);

#endif
