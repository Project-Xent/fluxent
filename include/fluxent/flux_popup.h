/**
 * @file flux_popup.h
 * @brief Popup window management for flyouts, tooltips, and menus.
 *
 * FluxPopup creates a child window that can display content outside
 * the main window bounds. Used as the foundation for Flyout, Tooltip,
 * and MenuFlyout implementations.
 */

#ifndef FLUX_POPUP_H
#define FLUX_POPUP_H

#include "flux_types.h"
#include "flux_window.h"
#include <stdbool.h>
#include "flux_graphics.h"
#include "flux_engine.h"
#include "flux_node_store.h"
#include "flux_input.h"
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Popup placement relative to anchor. */
typedef enum FluxPlacement
{
	FLUX_PLACEMENT_BOTTOM, /**< Below the anchor. */
	FLUX_PLACEMENT_TOP,    /**< Above the anchor. */
	FLUX_PLACEMENT_LEFT,   /**< Left of the anchor. */
	FLUX_PLACEMENT_RIGHT,  /**< Right of the anchor. */
	FLUX_PLACEMENT_AUTO,   /**< Automatic (best fit). */
} FluxPlacement;

typedef struct FluxPopup FluxPopup;

/**
 * Create a popup owned by the given window.
 * The popup creates a WS_POPUP child HWND with its own D2D render target.
 */
FluxPopup               *flux_popup_create(FluxWindow *owner);

/**
 * Destroy the popup and its associated resources.
 */
void                     flux_popup_destroy(FluxPopup *popup);

/**
 * Set the xent node subtree that represents the popup content.
 * The popup will render this subtree when visible.
 */
void flux_popup_set_content(FluxPopup *popup, XentContext *ctx, FluxNodeStore *store, XentNodeId content_root);

/**
 * Show the popup anchored to `anchor` rect (in screen coordinates)
 * with the given placement preference.
 */
void flux_popup_show(FluxPopup *popup, FluxRect anchor, FluxPlacement placement);

/**
 * Entrance-transition variants — each popup type should use the matching
 * WinUI theme transition. The values map 1:1 to WinUI:
 *
 *   MENU    → MenuPopupThemeTransition    (MenuFlyout, context menu)
 *             ScaleY 0.5→1 from anchor edge, 250 ms, cubic-bezier(0,0,0,1),
 *             no opacity fade.
 *
 *   FLYOUT  → PopupThemeTransition        (Flyout / FlyoutPresenter,
 *             CommandBarFlyout, DatePicker, etc.)
 *             Translate ±50 DIP along placement axis + Opacity 0→1,
 *             ~167 ms, ease-out.
 *
 *   TOOLTIP → Fade{In,Out}ThemeAnimation  (ToolTip)
 *             Opacity 0→1 only, 83 ms linear.
 *
 *   NONE    → instant show (no animation).
 *
 * Default for a freshly-created popup is MENU. Call immediately after
 * flux_popup_create() — before the first flux_popup_show().
 */
typedef enum FluxPopupAnimStyle
{
	FLUX_POPUP_ANIM_MENU,
	FLUX_POPUP_ANIM_FLYOUT,
	FLUX_POPUP_ANIM_TOOLTIP,
	FLUX_POPUP_ANIM_NONE,
} FluxPopupAnimStyle;

void         flux_popup_set_anim_style(FluxPopup *popup, FluxPopupAnimStyle style);

/**
 * Dismiss (hide) the popup.
 */
void         flux_popup_dismiss(FluxPopup *popup);

/**
 * Update popup position (call when the owner window moves/resizes).
 */
void         flux_popup_update_position(FluxPopup *popup);

/**
 * Returns true if the popup is currently visible.
 */
bool         flux_popup_is_visible(FluxPopup const *popup);

/**
 * Set the desired content size for the popup.
 * The popup window will be resized to fit this content.
 */
void         flux_popup_set_size(FluxPopup *popup, float width, float height);

/**
 * Set whether clicking outside the popup should dismiss it.
 * Default is true.
 */
void         flux_popup_set_dismiss_on_outside(FluxPopup *popup, bool dismiss);

/**
 * Set a callback for when the popup is dismissed.
 */
typedef void (*FluxPopupDismissCallback)(void *ctx);
void         flux_popup_set_dismiss_callback(FluxPopup *popup, FluxPopupDismissCallback cb, void *ctx);

/**
 * Paint callback — called during WM_PAINT after clearing.
 * The callback should draw tooltip/flyout content using the popup's graphics.
 */
typedef void (*FluxPopupPaintCallback)(void *ctx, FluxPopup *popup);
void         flux_popup_set_paint_callback(FluxPopup *popup, FluxPopupPaintCallback cb, void *ctx);

/**
 * Mouse event types forwarded from the popup window.
 */
typedef enum FluxPopupMouseEvent
{
	FLUX_POPUP_MOUSE_MOVE,
	FLUX_POPUP_MOUSE_DOWN,
	FLUX_POPUP_MOUSE_UP,
	FLUX_POPUP_MOUSE_LEAVE,
	FLUX_POPUP_MOUSE_WHEEL, /* x = cursor x, y = wheel delta (DIPs to scroll; +up, -down) */
} FluxPopupMouseEvent;

/**
 * Mouse callback — called when the popup receives mouse events.
 * Coordinates are in logical (DIP) space relative to the popup's top-left.
 */
typedef void  (*FluxPopupMouseCallback)(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y);
void          flux_popup_set_mouse_callback(FluxPopup *popup, FluxPopupMouseCallback cb, void *ctx);

/**
 * Get the popup's FluxGraphics — used by paint callbacks to access D2D context.
 */
FluxGraphics *flux_popup_get_graphics(FluxPopup *popup);

/**
 * Get the popup's HWND.
 */
HWND          flux_popup_get_hwnd(FluxPopup *popup);

/* ── System backdrop (Windows 11 DWM acrylic) ─────────────────── */

/**
 * Try to enable a Windows 11 transient acrylic system backdrop on the
 * popup HWND, along with DWM rounded corners (so the acrylic does not
 * bleed into the rectangular window corners) and the immersive dark-
 * mode flag when `is_dark` is true (so the acrylic tint matches the
 * app theme). When this succeeds the popup is meant to be drawn with
 * a fully transparent background so the DWM acrylic shows through.
 *
 * Returns true when the attribute was successfully applied. Returns
 * false on older systems / when the DWM backdrop is unavailable; the
 * caller should then fall back to a solid fill color.
 */
bool          flux_popup_enable_system_backdrop(FluxPopup *popup, bool is_dark);

/**
 * Query whether the popup currently has a working system backdrop.
 * Controls use this to decide whether to draw an opaque fallback or
 * leave the card transparent.
 */
bool          flux_popup_has_system_backdrop(FluxPopup const *popup);

/* ── Clamp to monitor work area (prevents off-screen overflow) ── */

/**
 * Set the maximum permitted popup content height in DIPs. The popup
 * will clamp its own size to this value at show time / position update,
 * which is the safest way to ensure the card never exceeds the monitor
 * work area. Pass 0 to disable the clamp (default).
 */
void          flux_popup_set_max_content_height(FluxPopup *popup, float max_h);

/* ── Owner-window interruption ───────────────────────────────── */

/**
 * Dismiss every visible popup whose owner HWND matches `owner_hwnd`.
 * Call this from the owner window's message handlers when an external
 * event should close any open flyout/menu/tooltip:
 *   - WM_ACTIVATE (WA_INACTIVE)  — window switch / Alt-Tab
 *   - WM_MOUSEWHEEL              — user scrolling the owner window
 *   - WM_MOVE / WM_SIZE          — owner window dragged / resized
 *
 * This matches WinUI's `LightDismissOverlayMode` behaviour where any
 * such interaction dismisses the transient surface.
 */
void          flux_popup_dismiss_all_for_owner(HWND owner_hwnd);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_POPUP_H */
