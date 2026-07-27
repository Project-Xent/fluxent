/**
 * @file flux_engine.h
 * @brief Render command collection and dispatch for Fluxent controls.
 *
 * The FluxEngine walks the Xent layout tree and collects render commands
 * for each control. These commands capture a snapshot of each control's
 * state, bounds, and visual properties at the moment of collection.
 *
 * ## Usage Pattern
 *
 * 1. Create engine with `flux_engine_create(store)`
 * 2. Each frame:
 *    - Call `flux_engine_collect(eng, ctx, root)` to walk the tree
 *    - Call `flux_engine_execute(eng, rc)` to render all controls
 * 3. Destroy with `flux_engine_destroy(eng)`
 *
 * ## Threading
 *
 * - Collection must happen on the main thread (accesses layout data)
 * - Execution must happen on the render thread (issues D2D calls)
 * - In single-threaded apps, these can be called sequentially
 *
 * Built-in controls paint through a private renderer table inside fluxent.
 * Components and HtmlView attach XentDl values replayed by XtkBackend.
 */
#ifndef FLUX_ENGINE_H
#define FLUX_ENGINE_H

#include "flux_render_snapshot.h"
#include <fluxent/flux_node_store.h>


typedef struct FluxRenderCtx FluxRenderCtx;
typedef struct FluxEngine    FluxEngine;
typedef struct XtkBackend    XtkBackend;

/**
 * @brief Simplified control state for rendering.
 *
 * Copied from FluxNodeState during collection. Renderers read these flags
 * to apply visual feedback (hover highlight, press effect, focus ring).
 */
typedef struct FluxControlState {
	uint8_t hovered;      /**< Pointer is over this control */
	uint8_t pressed;      /**< Pointer is pressed on this control */
	uint8_t focused;      /**< Control has keyboard focus */
	uint8_t enabled;      /**< Control is interactive */
	uint8_t pointer_type; /**< FluxPointerType: 0=mouse, 1=touch, 2=pen */
} FluxControlState;

/**
 * @brief Render phase for layered drawing.
 *
 * Main phase draws backgrounds, content. Overlay phase draws decorations
 * that must appear on top (scrollbars, selection handles, etc.).
 */
typedef enum FluxCommandPhase
{
	FLUX_PHASE_MAIN,    /**< Primary render pass */
	FLUX_PHASE_OVERLAY, /**< Secondary pass for top-layer decorations */
} FluxCommandPhase;

/**
 * @brief Clip region modification for hierarchical clipping.
 */
typedef enum FluxClipAction
{
	FLUX_CLIP_NONE,           /**< No clip change */
	FLUX_CLIP_PUSH,           /**< Push new clip region (+ scroll translate) onto stack */
	FLUX_CLIP_POP,            /**< Pop clip region from stack */
	FLUX_CLIP_PUSH_TRANSFORM, /**< Push a scale/translate transform + opacity layer (+ optional subtree clip) */
	FLUX_CLIP_POP_TRANSFORM,  /**< Pop the transform + opacity layer (+ optional subtree clip) */
} FluxClipAction;

/**
 * @brief A single render command in the command list.
 *
 * Contains everything needed to render one control: its snapshot,
 * computed bounds, interaction state, and clipping information.
 */
typedef struct FluxRenderCommand {
	FluxRenderSnap   snapshot;     /**< Control-specific render data */
	FluxRect         bounds;       /**< Layout-computed bounding rect */
	FluxControlState state;        /**< Interaction state at collection time */
	FluxCommandPhase phase;        /**< Which pass to render in */
	FluxClipAction   clip_action;  /**< Clip stack operation */
	float            scroll_x;     /**< Horizontal scroll offset */
	float            scroll_y;     /**< Vertical scroll offset */
	float            scale;        /**< Subtree scale for FLUX_CLIP_PUSH_TRANSFORM (1 = none) */
	float            pivot_x;      /**< Scale pivot X (absolute) */
	float            pivot_y;      /**< Scale pivot Y (absolute) */
	float            opacity;      /**< Subtree opacity for FLUX_CLIP_PUSH_TRANSFORM (1 = opaque) */
	float            translate_x;  /**< Subtree X translate for FLUX_CLIP_PUSH_TRANSFORM (0 = none) */
	float            translate_y;  /**< Subtree Y translate for FLUX_CLIP_PUSH_TRANSFORM (0 = none) */
	bool             clip_subtree; /**< Clip the subtree to @ref bounds (slide animations) */
} FluxRenderCommand;

/**
 * @brief Degraded-frame callback type.
 *
 * Called once at the end of a flux_engine_collect() frame in which a
 * collect-time allocation failed and part of the frame was dropped.
 */
typedef void               (*FluxDegradedFn)(void *ctx);

/**
 * @brief Create a render engine.
 * @param store Node store containing control metadata.
 * @return New engine instance, or NULL on failure.
 */
XENT_NODISCARD FluxEngine *flux_engine_create(FluxStore *store);

/**
 * @brief Destroy a render engine.
 * @param eng Engine to destroy (NULL is safe).
 */
void                       flux_engine_destroy(FluxEngine *eng);

/**
 * @brief Set the degraded-frame callback (NULL to clear).
 *
 * A degraded frame always logs an OutputDebugStringA line when collection
 * ends. With no callback registered, a degraded frame also asserts in debug
 * builds; registering a callback tells the engine the embedder handles
 * degradation itself.
 *
 * @param eng Engine instance.
 * @param cb Callback function.
 * @param ctx User context passed to callback.
 */
void                       flux_engine_ondegraded(FluxEngine *eng, FluxDegradedFn cb, void *ctx);

/**
 * @brief Collect render commands by walking the layout tree.
 *
 * Clears any previous commands and rebuilds the command list from the
 * current layout state. Must be called each frame before rendering.
 *
 * @param eng Engine instance.
 * @param ctx Xent layout context.
 * @param root Root node to start collection from.
 */
void                       flux_engine_collect(FluxEngine *eng, XentCtx *ctx, XentNodeId root);

/**
 * @brief Get the number of collected commands.
 * @param eng Engine instance.
 * @return Command count.
 */
uint32_t                   flux_engine_ncmd(FluxEngine const *eng);

/**
 * @brief Get a command by index.
 * @param eng Engine instance.
 * @param index Command index (0 to count-1).
 * @return Pointer to command, or NULL if out of range.
 */
FluxRenderCommand const   *flux_engine_cmd(FluxEngine const *eng, uint32_t index);

/**
 * @brief Execute all collected render commands.
 *
 * Issues D2D draw calls for each command in order. Must be called
 * between flux_gfx_begin() and flux_gfx_end().
 *
 * @param eng Engine instance.
 * @param rc Render context with D2D resources.
 */
void                       flux_engine_execute(FluxEngine const *eng, FluxRenderCtx const *rc);


#endif
