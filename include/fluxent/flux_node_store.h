/**
 * @file flux_node_store.h
 * @brief Node storage and per-node metadata for Fluxent UI components.
 *
 * FluxNodeStore provides a mapping from XentNodeId to FluxNodeData, enabling
 * the engine to associate visual properties, interaction state, and event
 * handlers with layout nodes.
 *
 * ## Architecture
 *
 * Each node's data is split into three conceptual areas:
 * - **FluxNodeVisuals**: Appearance (colors, corners, borders, opacity)
 * - **FluxNodeState**: Interaction flags (enabled, hovered, pressed, focused)
 * - **FluxNodeBehavior**: Event handler callbacks and their contexts
 *
 * The `component_data` pointer allows control-specific extensions (e.g.,
 * FluxButtonData, FluxSliderData) to be attached without polluting the
 * core structure.
 */
#ifndef FLUX_NODE_STORE_H
#define FLUX_NODE_STORE_H

#include "flux_types.h"
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Visual styling properties for a node.
 *
 * These properties control how the node is rendered (background fill,
 * border stroke, corner rounding, and overall opacity). They are typically
 * set during control creation and may be animated.
 */
typedef struct FluxNodeVisuals {
	FluxColor background;    /**< Fill color for the node's background */
	FluxColor border_color;  /**< Stroke color for the border */
	float     corner_radius; /**< Corner rounding radius in DIPs */
	float     border_width;  /**< Border stroke width in DIPs */
	float     opacity;       /**< Overall opacity [0.0, 1.0] */
} FluxNodeVisuals;

/**
 * @brief Transient interaction state for a node.
 *
 * These flags are updated by the input system and read by renderers to
 * apply visual feedback (hover highlights, press effects, focus rings).
 */
typedef struct FluxNodeState {
	uint8_t enabled       :1; /**< Control is interactable */
	uint8_t visible       :1; /**< Control should be rendered */
	uint8_t hovered       :1; /**< Pointer is over this node */
	uint8_t pressed       :1; /**< Pointer is pressed on this node */
	uint8_t focused       :1; /**< Node has keyboard focus */
	uint8_t dirty         :1; /**< Needs repaint (internal) */
	uint8_t suppress_focus:1; /**< Skip focus visuals (e.g., during animation) */
	/**
	 * Device type of the most recent pointer event (0=mouse, 1=touch, 2=pen).
	 * Renderers use this to apply touch-specific behavior (no hover visuals,
	 * larger thumbs, etc.).
	 */
	uint8_t pointer_type;
} FluxNodeState;

/**
 * @brief Event handlers for a node.
 *
 * Callbacks are invoked by the input system in response to user interaction.
 * Each callback has an associated context pointer passed as the first argument.
 * All callbacks are optional (NULL = no handler).
 */
typedef struct FluxNodeBehavior {
	/** @brief Click or tap completion callback. */
	void  (*on_click)(void *ctx);
	void *on_click_ctx;

	/** @brief Pointer movement callback in node-local coordinates. */
	void  (*on_pointer_move)(void *ctx, float x, float y);
	void *on_pointer_move_ctx;

	/** @brief Pointer down callback with click count. */
	void  (*on_pointer_down)(void *ctx, float x, float y, int click_count);
	void *on_pointer_down_ctx;

	/** @brief Focus gained callback. */
	void  (*on_focus)(void *ctx);
	void *on_focus_ctx;

	/** @brief Focus lost callback. */
	void  (*on_blur)(void *ctx);
	void *on_blur_ctx;

	/** @brief Key press or release callback. */
	void  (*on_key)(void *ctx, unsigned int vk, bool down);
	void *on_key_ctx;

	/** @brief Character input callback after keyboard translation. */
	void  (*on_char)(void *ctx, wchar_t ch);
	void *on_char_ctx;

	/** @brief IME composition update callback. */
	void  (*on_ime_composition)(void *ctx, wchar_t const *text, uint32_t len, uint32_t cursor);
	void *on_ime_composition_ctx;

	/** @brief Context menu callback. */
	void  (*on_context_menu)(void *ctx, float x, float y);
	void *on_context_menu_ctx;

	/**
	 * Pointer cancellation (capture lost, touch promoted to scroll-pan, etc.).
	 * Fires instead of on_click when the press is aborted.
	 * Use to release timers / reset state (e.g., RepeatButton).
	 */
	void  (*on_cancel)(void *ctx); /**< Pointer cancellation callback. */
	void *on_cancel_ctx;
} FluxNodeBehavior;

/**
 * @brief Complete metadata for a single UI node.
 *
 * Combines visual properties, interaction state, event handlers, and
 * control-specific data into one structure stored in FluxNodeStore.
 */
typedef struct FluxNodeData {
	XentNodeId       node_id;        /**< Associated layout node ID */
	FluxNodeVisuals  visuals;        /**< Appearance properties */
	FluxNodeState    state;          /**< Interaction flags */
	FluxNodeBehavior behavior;       /**< Event callbacks (embedded) */
	XentControlType  component_type; /**< Control type expected for component_data casts. */
	void            *component_data; /**< Control-specific data; borrowed unless destroy_component_data is set. */
	void             (*destroy_component_data)(void *component_data); /**< Optional owned component data destructor. */

	float            hover_local_x; /**< Sub-element hover X in node-local coordinates. */
	float            hover_local_y; /**< Sub-element hover Y in node-local coordinates. */

	char const      *tooltip_text;  /**< Tooltip text, or NULL when unset. */
} FluxNodeData;

typedef struct FluxNodeStore      FluxNodeStore;
typedef struct FluxRenderContext  FluxRenderContext;
typedef struct FluxRenderSnapshot FluxRenderSnapshot;
typedef struct FluxControlState   FluxControlState;

/** @brief Renderer callbacks for a control type, scoped to a node store. */
typedef struct FluxControlRenderer {
	void (*draw)(
	  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
	);
	void (*draw_overlay)(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds);
} FluxControlRenderer;

/**
 * @brief Create a new node store with the specified initial capacity.
 * @param initial_capacity Initial hash table capacity (will grow as needed).
 * @return New store, or NULL on allocation failure.
 */
FluxNodeStore *flux_node_store_create(uint32_t initial_capacity);

/**
 * @brief Destroy a node store and free all associated memory.
 * @param store Store to destroy (NULL is safe).
 */
void           flux_node_store_destroy(FluxNodeStore *store);

/**
 * @brief Look up node data by ID.
 * @param store Store to search.
 * @param id Node ID to find.
 * @return Pointer to FluxNodeData, or NULL if not found.
 */
FluxNodeData  *flux_node_store_get(FluxNodeStore *store, XentNodeId id);

/**
 * @brief Get or create node data for the given ID.
 * @param store Store to search/insert.
 * @param id Node ID.
 * @return Pointer to existing or newly-created FluxNodeData.
 */
FluxNodeData  *flux_node_store_get_or_create(FluxNodeStore *store, XentNodeId id);

/**
 * @brief Remove a node from the store.
 * @param store Store to modify.
 * @param id Node ID to remove.
 */
void           flux_node_store_remove(FluxNodeStore *store, XentNodeId id);

/**
 * @brief Get the number of nodes in the store.
 * @param store Store to query.
 * @return Number of stored nodes.
 */
uint32_t       flux_node_store_count(FluxNodeStore const *store);

/**
 * @brief Attach FluxNodeData pointers as userdata to all nodes in a context.
 *
 * After calling this, `xent_get_userdata(ctx, id)` returns the corresponding
 * FluxNodeData pointer for quick access during layout/render.
 *
 * @param store Store containing the node data.
 * @param ctx XentContext to attach userdata to.
 */
void           flux_node_store_attach_userdata(FluxNodeStore *store, XentContext *ctx);

/**
 * @brief Register a renderer in this store's isolated renderer table.
 */
void           flux_node_store_register_renderer(
  FluxNodeStore *store, XentControlType type,
  void (*draw)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *),
  void (*draw_overlay)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *)
);

/**
 * @brief Look up a renderer from this store's isolated renderer table.
 */
FluxControlRenderer const *flux_node_store_get_renderer(FluxNodeStore const *store, XentControlType type);

#ifdef __cplusplus
}
#endif

#endif
