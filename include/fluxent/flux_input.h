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

#include <windows.h>
#include "flux_component_data.h"
#include "flux_node_store.h"
#include "flux_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Result of a hit-test operation. */
typedef struct FluxHitResult {
	XentNodeId    node;   /**< The hit node ID. */
	FluxNodeData *data;   /**< Pointer to node data. */
	FluxRect      bounds; /**< Node bounds in parent coords. */
	FluxPoint     local;  /**< Hit point in node-local coords. */
} FluxHitResult;

typedef struct FluxInput FluxInput;

/** @brief Create an input router for a layout context and node store. */
FluxInput               *flux_input_create(XentContext *ctx, FluxNodeStore *store);
/** @brief Destroy an input router. */
void                     flux_input_destroy(FluxInput *input);

/** @brief Hit-test a point in root-local DIPs. */
FluxHitResult            flux_input_hit_test(FluxInput *input, XentNodeId root, float px, float py);

/** @brief Dispatch one unified pointer event through hit-testing and capture logic. */
void                     flux_input_dispatch(FluxInput *input, XentNodeId root, FluxPointerEvent const *ev);

/** @brief Dispatch a key-down event to the focused node. */
void                     flux_input_key_down(FluxInput *input, unsigned int vk);
/** @brief Dispatch character input to the focused node. */
void                     flux_input_char(FluxInput *input, wchar_t ch);

/** @brief Forward IME composition text to the focused node. */
void            flux_input_ime_composition(FluxInput *input, wchar_t const *text, uint32_t length, uint32_t cursor);
/** @brief End the current IME composition. */
void            flux_input_ime_end(FluxInput *input);

/** @brief Return the click count from the last pointer down. */
int             flux_input_get_click_count(FluxInput const *input);

/** @brief Return the currently focused node. */
XentNodeId      flux_input_get_focused(FluxInput const *input);
/** @brief Return the currently hovered node. */
XentNodeId      flux_input_get_hovered(FluxInput const *input);
/** @brief Return the currently pressed node. */
XentNodeId      flux_input_get_pressed(FluxInput const *input);

/** @brief Return the scrollable ancestor selected for touch-pan promotion. */
XentNodeId      flux_input_get_touch_pan_target(FluxInput const *input);

/** @brief Clear any in-progress touch-pan state. */
void            flux_input_clear_touch_pan(FluxInput *input);

/** @brief Read and clear the touch-pan promotion edge flag. */
bool            flux_input_take_pan_promoted(FluxInput *input);

/** @brief Return the device type of the most recent pointer event. */
FluxPointerType flux_input_get_pointer_type(FluxInput const *input);

/** @brief Return the id of the most recent pointer event. */
uint32_t        flux_input_get_pointer_id(FluxInput const *input);

/** @brief Cycle focus among all visible focusable nodes by tab order. */
void            flux_input_tab(FluxInput *input, XentNodeId root, bool shift);

/** @brief Move focus among siblings in the given arrow-key direction. */
void            flux_input_arrow(FluxInput *input, XentNodeId root, int direction);

/** @brief Trigger the focused node's activation behavior. */
void            flux_input_activate(FluxInput *input);

/** @brief Blur the current focus. */
void            flux_input_escape(FluxInput *input);

/** @brief Set focus to a specific node programmatically. */
void            flux_input_set_focus(FluxInput *input, XentNodeId node);

#ifdef __cplusplus
}
#endif

#endif
