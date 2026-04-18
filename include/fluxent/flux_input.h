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

/* FluxPointerType / FluxPointerEvent / FluxPointerEventKind / FluxPointerButton
 * are declared in flux_types.h so flux_window.h and flux_input.h share them
 * without mutual inclusion. */

/** @brief Result of a hit-test operation. */
typedef struct FluxHitResult {
	XentNodeId    node;   /**< The hit node ID. */
	FluxNodeData *data;   /**< Pointer to node data. */
	FluxRect      bounds; /**< Node bounds in parent coords. */
	FluxPoint     local;  /**< Hit point in node-local coords. */
} FluxHitResult;

typedef struct FluxInput FluxInput;

FluxInput               *flux_input_create(XentContext *ctx, FluxNodeStore *store);
void                     flux_input_destroy(FluxInput *input);

FluxHitResult            flux_input_hit_test(FluxInput *input, XentNodeId root, float px, float py);

/* Single entry for every pointer event.  The router decides:
 *  – which control (if any) captures the press,
 *  – whether a touch contact is escalated to scroll-pan,
 *  – whether right-click / long-press dispatches to on_context_menu,
 *  – how wheel deltas are routed to scroll containers,
 *  – and which node receives hover.
 * Controls never see this struct directly; they still receive their
 * typed callbacks (on_pointer_down/up/move/wheel/context_menu).          */
void                     flux_input_dispatch(FluxInput *input, XentNodeId root, FluxPointerEvent const *ev);

void                     flux_input_key_down(FluxInput *input, unsigned int vk);
void                     flux_input_char(FluxInput *input, wchar_t ch);

/* IME composition text forwarding (called from WM_IME_COMPOSITION) */
void            flux_input_ime_composition(FluxInput *input, wchar_t const *text, uint32_t length, uint32_t cursor);
void            flux_input_ime_end(FluxInput *input);

/* Returns the click count from the last pointer_down (1=single, 2=double, 3+=triple) */
int             flux_input_get_click_count(FluxInput const *input);

XentNodeId      flux_input_get_focused(FluxInput const *input);
XentNodeId      flux_input_get_hovered(FluxInput const *input);
XentNodeId      flux_input_get_pressed(FluxInput const *input);

/* Nearest scrollable ancestor of the most recent touch press, or
 * XENT_NODE_INVALID if the last press wasn't touch / nothing scrollable
 * was under it.  Used by the DManip integration to hand off the pointer
 * contact to the right viewport. */
XentNodeId      flux_input_get_touch_pan_target(FluxInput const *input);

/* Abandon the in-progress manual touch-pan.  Call this after successfully
 * handing the contact to DirectManipulation so subsequent MOVE events
 * don't also translate scroll_x/y by hand (DManip is now authoritative). */
void            flux_input_clear_touch_pan(FluxInput *input);

/* Read-and-clear the "touch-pan just crossed the slop threshold" edge
 * flag.  app_pointer polls this after every MOVE dispatch — a true
 * return means "the inner control was just canceled and a pan started",
 * which is the right moment to hand the pointer contact off to
 * DirectManipulation. */
bool            flux_input_take_pan_promoted(FluxInput *input);

/* Pointer device type of the most recent pointer event.
 * Controls / app code can branch on this to apply touch-specific behavior
 * (e.g. suppress tooltips, skip hover visuals, enlarge hit targets). */
FluxPointerType flux_input_get_pointer_type(FluxInput const *input);

/* Pointer id of the most recent pointer event (0 if synthesized /
 * unknown).  DManip integration reads this to hand off the contact. */
uint32_t        flux_input_get_pointer_id(FluxInput const *input);

/* ---- Focus Navigation (keyboard Tab/Arrow/Enter/Escape) ---- */

/** Tab key: cycle focus among all visible focusable nodes by tab_index order.
 *  shift=true → reverse direction (Shift+Tab). */
void            flux_input_tab(FluxInput *input, XentNodeId root, bool shift);

/** Arrow key: move focus among siblings in the given direction.
 *  direction: VK_LEFT=0x25, VK_UP=0x26, VK_RIGHT=0x27, VK_DOWN=0x28 */
void            flux_input_arrow(FluxInput *input, XentNodeId root, int direction);

/** Enter/Space: trigger on_click of the currently focused node. */
void            flux_input_activate(FluxInput *input);

/** Escape: blur the current focus (or dismiss context, handled by caller). */
void            flux_input_escape(FluxInput *input);

/** Set focus to a specific node programmatically. */
void            flux_input_set_focus(FluxInput *input, XentNodeId node);

#ifdef __cplusplus
}
#endif

#endif
