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

/* ═══════════════════════════════════════════════════════════════════════
   FluxNodeVisuals — Appearance properties
   ═══════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════
   FluxNodeState — Interaction state flags
   ═══════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════
   FluxNodeBehavior — Event handler callbacks
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Event handlers for a node.
 *
 * Callbacks are invoked by the input system in response to user interaction.
 * Each callback has an associated context pointer passed as the first argument.
 * All callbacks are optional (NULL = no handler).
 */
typedef struct FluxNodeBehavior {
	/* Click/tap completion */
	void  (*on_click)(void *ctx);
	void *on_click_ctx;

	/* Pointer movement while over this node (local coordinates) */
	void  (*on_pointer_move)(void *ctx, float x, float y);
	void *on_pointer_move_ctx;

	/* Pointer down with click count (1=single, 2=double, 3=triple) */
	void  (*on_pointer_down)(void *ctx, float x, float y, int click_count);
	void *on_pointer_down_ctx;

	/* Focus gained */
	void  (*on_focus)(void *ctx);
	void *on_focus_ctx;

	/* Focus lost */
	void  (*on_blur)(void *ctx);
	void *on_blur_ctx;

	/* Key press/release (vk = virtual key code) */
	void  (*on_key)(void *ctx, unsigned int vk, bool down);
	void *on_key_ctx;

	/* Character input (after keyboard translation) */
	void  (*on_char)(void *ctx, wchar_t ch);
	void *on_char_ctx;

	/* IME composition update */
	void  (*on_ime_composition)(void *ctx, wchar_t const *text, uint32_t len, uint32_t cursor);
	void *on_ime_composition_ctx;

	/* Right-click / long-press context menu */
	void  (*on_context_menu)(void *ctx, float x, float y);
	void *on_context_menu_ctx;

	/**
	 * Pointer cancellation (capture lost, touch promoted to scroll-pan, etc.).
	 * Fires instead of on_click when the press is aborted.
	 * Use to release timers / reset state (e.g., RepeatButton).
	 */
	void  (*on_cancel)(void *ctx);
	void *on_cancel_ctx;
} FluxNodeBehavior;

/* ═══════════════════════════════════════════════════════════════════════
   FluxNodeData — Complete per-node metadata
   ═══════════════════════════════════════════════════════════════════════ */

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
	void            *component_data; /**< Control-specific data (e.g., FluxButtonData) */

	/* Sub-element hover tracking (local coordinates within the node's bounds).
	   Updated by flux_input_pointer_move so renderers can determine which
	   internal zone the cursor is over (e.g., NumberBox spin buttons). */
	float            hover_local_x;
	float            hover_local_y;

	/* Tooltip text (shown on hover after delay). NULL = no tooltip. */
	char const      *tooltip_text;
} FluxNodeData;

/* ═══════════════════════════════════════════════════════════════════════
   FluxNodeStore API
   ═══════════════════════════════════════════════════════════════════════ */

typedef struct FluxNodeStore FluxNodeStore;

/**
 * @brief Create a new node store with the specified initial capacity.
 * @param initial_capacity Initial hash table capacity (will grow as needed).
 * @return New store, or NULL on allocation failure.
 */
FluxNodeStore               *flux_node_store_create(uint32_t initial_capacity);

/**
 * @brief Destroy a node store and free all associated memory.
 * @param store Store to destroy (NULL is safe).
 */
void                         flux_node_store_destroy(FluxNodeStore *store);

/**
 * @brief Look up node data by ID.
 * @param store Store to search.
 * @param id Node ID to find.
 * @return Pointer to FluxNodeData, or NULL if not found.
 */
FluxNodeData                *flux_node_store_get(FluxNodeStore *store, XentNodeId id);

/**
 * @brief Get or create node data for the given ID.
 * @param store Store to search/insert.
 * @param id Node ID.
 * @return Pointer to existing or newly-created FluxNodeData.
 */
FluxNodeData                *flux_node_store_get_or_create(FluxNodeStore *store, XentNodeId id);

/**
 * @brief Remove a node from the store.
 * @param store Store to modify.
 * @param id Node ID to remove.
 */
void                         flux_node_store_remove(FluxNodeStore *store, XentNodeId id);

/**
 * @brief Get the number of nodes in the store.
 * @param store Store to query.
 * @return Number of stored nodes.
 */
uint32_t                     flux_node_store_count(FluxNodeStore const *store);

/**
 * @brief Attach FluxNodeData pointers as userdata to all nodes in a context.
 *
 * After calling this, `xent_get_userdata(ctx, id)` returns the corresponding
 * FluxNodeData pointer for quick access during layout/render.
 *
 * @param store Store containing the node data.
 * @param ctx XentContext to attach userdata to.
 */
void                         flux_node_store_attach_userdata(FluxNodeStore *store, XentContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_NODE_STORE_H */
