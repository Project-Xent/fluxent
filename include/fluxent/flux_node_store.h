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
#include "flux_control_type.h"

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

	/** @brief Key callback; returns true if the control consumed the key (suppressing
	 * app-level fallbacks like focus navigation or activation). */
	bool  (*on_key)(void *ctx, unsigned int vk, bool down);
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

	/** @brief Middle-button release on the node (e.g. TabViewItem close). */
	void  (*on_middle_click)(void *ctx);
	void *on_middle_click_ctx;

	/** @brief Pointer entered (true) / left (false) the node — for visibility
	 * toggles that must outlive the paint pass (e.g. TabViewItem's close button). */
	void  (*on_hover_changed)(void *ctx, bool hovered);
	void *on_hover_changed_ctx;
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
	FluxControlType  component_type; /**< Control type expected for component_data casts. */
	void            *component_data; /**< Control-specific data; borrowed unless destroy_component_data is set. */
	void             (*destroy_component_data)(void *component_data); /**< Optional owned component data destructor. */

	float            hover_local_x; /**< Sub-element hover X in node-local coordinates. */
	float            hover_local_y; /**< Sub-element hover Y in node-local coordinates. */

	char const      *tooltip_text;  /**< Owned tooltip text copy, or NULL when unset. */

	/**
	 * Transient render transform applied to this node's subtree (entrance/exit
	 * animations, e.g. ContentDialog). Both default to 1.0 (identity); the engine
	 * wraps the subtree in a scale-about-center + opacity layer only when either
	 * differs from 1.0 (or render_translate_y is non-zero), so untouched nodes
	 * incur no cost.
	 */
	float            render_scale;
	float            render_opacity;
	float            render_translate_x;  /**< Subtree X translate in px (0 = none); for drag visuals. */
	float            render_translate_y;  /**< Subtree Y translate in px (0 = none); for slide animations. */
	bool             render_clip_subtree; /**< Clip the subtree to this node's layout rect while transformed. */
	bool             clips_children;      /**< Clip children to this node's rect (e.g. NavView's off-screen
	                                       * Minimal pane), independent of any render transform. */
} FluxNodeData;

typedef struct FluxNodeStore FluxNodeStore;

/**
 * @brief Create a new node store with the specified initial capacity.
 * @param initial_capacity Initial hash table capacity (will grow as needed).
 * @return New store, or NULL on allocation failure.
 */
FluxNodeStore               *flux_node_store_create(uint32_t initial_capacity);

/**
 * @brief Bind the XentContext this store's nodes live in.
 *
 * Lets store-only APIs (e.g. flux_*_set_enabled) reach context-scoped node
 * properties such as semantic enabled. Set once, at scene creation, before the
 * tree is built. Safe to call again (idempotent).
 */
void                         flux_node_store_bind_context(FluxNodeStore *store, XentContext *ctx);

/** @brief The XentContext bound via flux_node_store_bind_context (NULL if unbound). */
XentContext                 *flux_node_store_context(FluxNodeStore const *store);

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

/** @brief Notified for each destroyed node before its store data is freed. */
typedef void                 (*FluxNodeRemovedFn)(void *userdata, XentNodeId id);

/**
 * @brief Register the single node-removed listener (NULL clears it).
 *
 * Fires once per node freed via xent_destroy_node / flux_subtree_destroy,
 * before the store entry (and component data) is released, so the listener
 * can drop its references and free node-owned platform resources. The node
 * is already invalid in xent when this fires; read identity from the store.
 */
void     flux_node_store_set_remove_listener(FluxNodeStore *store, FluxNodeRemovedFn fn, void *userdata);

/**
 * @brief Destroy a node and its entire subtree in one call.
 *
 * Detaches the node from its parent, then frees every node in the subtree:
 * each one's component data is destroyed through the store and the removed
 * listener (input/popup reference invalidation) fires per node. This is the
 * teardown path for page swaps and the declarative reconciler's unmount.
 * Requires flux_node_store_bind_context (FluxApp scenes do this).
 */
void     flux_subtree_destroy(FluxNodeStore *store, XentNodeId node);

/**
 * @brief Get the number of nodes in the store.
 * @param store Store to query.
 * @return Number of stored nodes.
 */
uint32_t flux_node_store_count(FluxNodeStore const *store);

/**
 * @brief Attach FluxNodeData pointers as userdata to all nodes in a context.
 *
 * After calling this, `xent_get_userdata(ctx, id)` returns the corresponding
 * FluxNodeData pointer for quick access during layout/render.
 *
 * @param store Store containing the node data.
 * @param ctx XentContext to attach userdata to.
 */
void     flux_node_store_attach_userdata(FluxNodeStore *store, XentContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
