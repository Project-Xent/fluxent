/**
 * @file flux_input.h
 * @brief Input handling, hit-testing, and focus management.
 *
 * FluxInput processes pointer events from FluxWindow and dispatches
 * them to the appropriate control based on hit-testing against the
 * layout tree. Handles focus, capture, and event bubbling.
 */

#ifndef FLUX_INPUT_H
#define FLUX_INPUT_H

#include <stddef.h>
#include "flux_node_store.h"
#include "flux_types.h"

/** @brief Result of a hit-test operation. */
typedef struct FluxHitResult {
	XentNodeId    node;   /**< The hit node ID. */
	FluxNodeData *data;   /**< Pointer to node data. */
	FluxRect      bounds; /**< Node bounds in parent coords. */
	FluxPoint     local;  /**< Hit point in node-local coords. */
} FluxHitResult;

typedef struct FluxInput  FluxInput;

/** @brief Create an input router for a layout context and node store. */
XENT_NODISCARD FluxInput *flux_input_create(XentCtx *ctx, FluxStore *store);
/** @brief Destroy an input router. */
void                      flux_input_destroy(FluxInput *input);

/** @brief Hit-test a point in root-local DIPs. */
FluxHitResult             flux_input_hittest(FluxInput *input, XentNodeId root, float px, float py);

/** @brief Dispatch one unified pointer event through hit-testing and capture logic. */
void                      flux_input_dispatch(FluxInput *input, XentNodeId root, FluxPointerEvent const *ev);

/** @brief Dispatch a key-down event to the focused node. Returns true if consumed. */
bool                      flux_input_keydown(FluxInput *input, FluxKey key);
/** @brief Dispatch a key-up event to the focused node (e.g. to stop a RepeatButton). */
void                      flux_input_keyup(FluxInput *input, FluxKey key);
/** @brief Dispatch character input to the focused node. */
void                      flux_input_char(FluxInput *input, wchar_t ch);

/** @brief Forward IME composition text to the focused node. */
void                      flux_input_ime(FluxInput *input, wchar_t const *text, uint32_t length, uint32_t cursor);
/** @brief End the current IME composition. */
void                      flux_input_imeend(FluxInput *input);

/** @brief Return the click count from the last pointer down. */
int                       flux_input_clicks(FluxInput const *input);

/** @brief Return the currently focused node. */
XentNodeId                flux_input_focus(FluxInput const *input);
/** @brief Return the currently hovered node. */
XentNodeId                flux_input_hover(FluxInput const *input);
/** @brief Return the currently pressed node. */
XentNodeId                flux_input_pressed(FluxInput const *input);

/** @brief Return whether the mouse pointer is inside a node's arranged bounds. */
bool                      flux_input_pointer_inside(FluxInput const *input, XentNodeId node);

/** @brief Return the scrollable ancestor selected for touch-pan promotion. */
XentNodeId                flux_input_pantarget(FluxInput const *input);

/** @brief Clear any in-progress touch-pan state. */
void                      flux_input_clearpan(FluxInput *input);

/** @brief Read and clear the touch-pan promotion edge flag. */
bool                      flux_input_takepan(FluxInput *input);

/** @brief Return the device type of the most recent pointer event. */
FluxPointerType           flux_input_pointertype(FluxInput const *input);

/** @brief Return the id of the most recent pointer event. */
uint32_t                  flux_input_pointerid(FluxInput const *input);

/** @brief Cycle focus among all visible focusable nodes by tab order. */
void                      flux_input_tab(FluxInput *input, XentNodeId root, bool shift);

/** @brief Move focus among siblings in the given arrow-key direction. */
void                      flux_input_arrow(FluxInput *input, XentNodeId root, FluxKey direction);

/** @brief Trigger the focused node's activation behavior. */
void                      flux_input_activate(FluxInput *input);

/** @brief Blur the current focus. */
void                      flux_input_escape(FluxInput *input);

/**
 * @brief Set focus to a specific node programmatically (WinUI FocusState_Programmatic).
 *
 * Sets logical focus and raises GotFocus but does not engage the keyboard focus
 * visual: the focus rectangle appears only once the user navigates by keyboard
 * (Tab/arrows), the same rule as pointer focus (where only a text input shows a
 * visual, its caret). This is why a ContentDialog's default button is focused on
 * open without a focus ring drawn around it.
 */
void                      flux_input_setfocus(FluxInput *input, XentNodeId node);
void                      flux_input_setfocus_programmatic(FluxInput *input, XentNodeId node);

/**
 * @brief Drop every reference to a destroyed node (focus, hover, capture,
 * scroll sessions, modal trap) and release its DManip viewport. No blur or
 * hover callbacks fire while the node's behavior state is being destroyed.
 * Wired to the node store's remove listener by FluxApp.
 */
void                      flux_input_dropnode(FluxInput *input, XentNodeId node);

/**
 * @brief Install or clear a modal focus trap (used by ContentDialog).
 *
 * While @p root is valid, Tab navigation is confined to its subtree and Escape
 * invokes @p on_escape instead of clearing focus. Pass XENT_NODE_INVALID and
 * NULL to remove the trap.
 */
void flux_input_setmodal(FluxInput *input, XentNodeId root, void (*on_escape)(void *ctx), void *ctx);


#endif
