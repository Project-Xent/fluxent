#ifndef FLUX_INPUT_H
#define FLUX_INPUT_H

#include <windows.h>
#include "flux_component_data.h"
#include "flux_node_store.h"
#include "flux_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxHitResult {
    XentNodeId    node;
    FluxNodeData *data;
    FluxRect      bounds;
    FluxPoint     local;
} FluxHitResult;

typedef struct FluxInput FluxInput;

FluxInput    *flux_input_create(XentContext *ctx, FluxNodeStore *store);
void          flux_input_destroy(FluxInput *input);

FluxHitResult flux_input_hit_test(FluxInput *input, XentNodeId root,
                                  float px, float py);

void          flux_input_pointer_down(FluxInput *input, XentNodeId root,
                                      float px, float py);

void          flux_input_pointer_up(FluxInput *input, XentNodeId root,
                                    float px, float py);

void          flux_input_pointer_move(FluxInput *input, XentNodeId root,
                                      float px, float py);

void          flux_input_key_down(FluxInput *input, unsigned int vk);
void          flux_input_char(FluxInput *input, wchar_t ch);

/* IME composition text forwarding (called from WM_IME_COMPOSITION) */
void          flux_input_ime_composition(FluxInput *input, const wchar_t *text,
                                         uint32_t length, uint32_t cursor);
void          flux_input_ime_end(FluxInput *input);

/* Right-click context menu (called from WM_RBUTTONUP on focused text input) */
void          flux_input_context_menu(FluxInput *input, XentNodeId root,
                                      float px, float py, HWND hwnd);

/* Returns the click count from the last pointer_down (1=single, 2=double, 3+=triple) */
int           flux_input_get_click_count(const FluxInput *input);

XentNodeId    flux_input_get_focused(const FluxInput *input);
XentNodeId    flux_input_get_hovered(const FluxInput *input);
XentNodeId    flux_input_get_pressed(const FluxInput *input);

void          flux_input_scroll(FluxInput *input, XentNodeId root, float px, float py, float delta);

/* ---- Focus Navigation (keyboard Tab/Arrow/Enter/Escape) ---- */

/** Tab key: cycle focus among all visible focusable nodes by tab_index order.
 *  shift=true → reverse direction (Shift+Tab). */
void flux_input_tab(FluxInput *input, XentNodeId root, bool shift);

/** Arrow key: move focus among siblings in the given direction.
 *  direction: VK_LEFT=0x25, VK_UP=0x26, VK_RIGHT=0x27, VK_DOWN=0x28 */
void flux_input_arrow(FluxInput *input, XentNodeId root, int direction);

/** Enter/Space: trigger on_click of the currently focused node. */
void flux_input_activate(FluxInput *input);

/** Escape: blur the current focus (or dismiss context, handled by caller). */
void flux_input_escape(FluxInput *input);

/** Set focus to a specific node programmatically. */
void flux_input_set_focus(FluxInput *input, XentNodeId node);

#ifdef __cplusplus
}
#endif

#endif