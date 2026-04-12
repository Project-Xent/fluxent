#ifndef FLUX_POPUP_H
#define FLUX_POPUP_H

#include "flux_types.h"
#include "flux_window.h"
#include "flux_graphics.h"
#include "flux_engine.h"
#include "flux_node_store.h"
#include "flux_input.h"
#include <xent/xent.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FluxPlacement {
    FLUX_PLACEMENT_BOTTOM,
    FLUX_PLACEMENT_TOP,
    FLUX_PLACEMENT_LEFT,
    FLUX_PLACEMENT_RIGHT,
    FLUX_PLACEMENT_AUTO,
} FluxPlacement;

typedef struct FluxPopup FluxPopup;

/**
 * Create a popup owned by the given window.
 * The popup creates a WS_POPUP child HWND with its own D2D render target.
 */
FluxPopup *flux_popup_create(FluxWindow *owner);

/**
 * Destroy the popup and its associated resources.
 */
void flux_popup_destroy(FluxPopup *popup);

/**
 * Set the xent node subtree that represents the popup content.
 * The popup will render this subtree when visible.
 */
void flux_popup_set_content(FluxPopup *popup, XentContext *ctx,
                            FluxNodeStore *store, XentNodeId content_root);

/**
 * Show the popup anchored to `anchor` rect (in screen coordinates)
 * with the given placement preference.
 */
void flux_popup_show(FluxPopup *popup, FluxRect anchor, FluxPlacement placement);

/**
 * Dismiss (hide) the popup.
 */
void flux_popup_dismiss(FluxPopup *popup);

/**
 * Update popup position (call when the owner window moves/resizes).
 */
void flux_popup_update_position(FluxPopup *popup);

/**
 * Returns true if the popup is currently visible.
 */
bool flux_popup_is_visible(const FluxPopup *popup);

/**
 * Set the desired content size for the popup.
 * The popup window will be resized to fit this content.
 */
void flux_popup_set_size(FluxPopup *popup, float width, float height);

/**
 * Set whether clicking outside the popup should dismiss it.
 * Default is true.
 */
void flux_popup_set_dismiss_on_outside(FluxPopup *popup, bool dismiss);

/**
 * Set a callback for when the popup is dismissed.
 */
typedef void (*FluxPopupDismissCallback)(void *ctx);
void flux_popup_set_dismiss_callback(FluxPopup *popup, FluxPopupDismissCallback cb, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_POPUP_H */