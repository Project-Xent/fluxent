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
#include "flux_node_store.h"
#include "flux_input.h"
#include <xent/xent.h>
#include <xent/xent_display.h>

/** @brief Popup placement relative to anchor. */
typedef enum FluxPlacement
{
	FLUX_PLACEMENT_BOTTOM, /**< Below the anchor. */
	FLUX_PLACEMENT_TOP,    /**< Above the anchor. */
	FLUX_PLACEMENT_LEFT,   /**< Left of the anchor. */
	FLUX_PLACEMENT_RIGHT,  /**< Right of the anchor. */
	FLUX_PLACEMENT_AUTO,   /**< Automatic (best fit). */
} FluxPlacement;

typedef struct FluxPopup  FluxPopup;
typedef struct FluxText   FluxText;

/**
 * @brief Create a popup owned by the given window.
 * The popup creates a WS_POPUP child HWND with its own D2D render target.
 */
XENT_NODISCARD FluxPopup *flux_popup_create(FluxWindow *owner);

/**
 * @brief Destroy the popup and its associated resources.
 */
void                      flux_popup_destroy(FluxPopup *popup);

/**
 * @brief Set the xent node subtree that represents the popup content.
 * The popup will render this subtree when visible.
 */
void flux_popup_set_content(FluxPopup *popup, XentCtx *ctx, FluxStore *store, XentNodeId content_root);

/**
 * @brief Show the popup anchored to `anchor` rect (in screen coordinates)
 * with the given placement preference.
 */
void flux_popup_show(FluxPopup *popup, FluxRect anchor, FluxPlacement placement);

/**
 * @brief Entrance transition style used by popup surfaces.
 */
typedef enum FluxPopupAnimStyle
{
	FLUX_POPUP_ANIM_MENU,
	FLUX_POPUP_ANIM_FLYOUT,
	FLUX_POPUP_ANIM_TOOLTIP,
	FLUX_POPUP_ANIM_NONE,
} FluxPopupAnimStyle;

/** @brief Set the entrance transition style before showing the popup. */
void flux_popup_setanim(FluxPopup *popup, FluxPopupAnimStyle style);

/**
 * @brief MenuPopupThemeTransition values for renderers that split border/content drawing.
 */
typedef struct FluxPopupMenuTransition {
	bool  active;
	float border_scale_y;
	float border_center_y;
	float content_translate_y;
	float clip_translate_y;
} FluxPopupMenuTransition;

/** @brief Resolve WinUI-style MenuPopupThemeTransition values for an opened menu length. */
FluxPopupMenuTransition flux_popup_menutransition(FluxPopup const *popup, float opened_length);

/**
 * @brief Dismiss (hide) the popup.
 */
void                    flux_popup_dismiss(FluxPopup *popup);

/**
 * @brief Update popup position (call when the owner window moves/resizes).
 */
void                    flux_popup_update_position(FluxPopup *popup);

/**
 * @brief Returns true if the popup is currently visible.
 */
bool                    flux_popup_visible(FluxPopup const *popup);

/**
 * @brief Set the desired content size for the popup.
 * The popup window will be resized to fit this content.
 */
void                    flux_popup_set_size(FluxPopup *popup, float width, float height);

/**
 * @brief Set whether clicking outside the popup should dismiss it.
 * Default is true.
 */
void                    flux_popup_setdismissoutside(FluxPopup *popup, bool dismiss);

/**
 * @brief Callback invoked when a popup is dismissed.
 */
typedef void            (*FluxPopupDismissFn)(void *ctx);
/** @brief Set a callback for when the popup is dismissed. */
void                    flux_popup_ondismiss(FluxPopup *popup, FluxPopupDismissFn cb, void *ctx);

/**
 * @brief Paint callback called while the popup is drawing.
 * The callback should draw tooltip/flyout content using the popup's graphics.
 */
typedef void            (*FluxPopupPaintFn)(void *ctx, FluxPopup *popup);
/** @brief Set the popup paint callback. */
void                    flux_popup_onpaint(FluxPopup *popup, FluxPopupPaintFn cb, void *ctx);

/* The returned list transfers to the popup and is freed after synchronous replay. */
typedef XentDl         *(*FluxPopupRecordFn)(void *ctx, FluxPopup const *popup);
void                    flux_popup_onrecord(FluxPopup *popup, FluxPopupRecordFn cb, void *ctx, FluxText *text);

/**
 * @brief Mouse event types forwarded from the popup window.
 */
typedef enum FluxPopupMouseEvent
{
	FLUX_POPUP_MOUSE_MOVE,
	FLUX_POPUP_MOUSE_DOWN,
	FLUX_POPUP_MOUSE_UP,
	FLUX_POPUP_MOUSE_LEAVE,
	FLUX_POPUP_MOUSE_WHEEL, /**< x = cursor x, y = wheel delta in DIPs. */
} FluxPopupMouseEvent;

/**
 * @brief Mouse callback called when the popup receives mouse events.
 * Coordinates are in logical (DIP) space relative to the popup's top-left.
 */
typedef void (*FluxPopupMouseFn)(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y);
/** @brief Set the popup mouse callback. */
void         flux_popup_onmouse(FluxPopup *popup, FluxPopupMouseFn cb, void *ctx);

/**
 * @brief Get the popup's FluxGfx for paint callbacks.
 */
FluxGfx     *flux_popup_gfx(FluxPopup *popup);

/**
 * @brief Get the popup's HWND.
 */
HWND         flux_popup_hwnd(FluxPopup *popup);

/**
 * @brief Whether the most recent mouse event delivered to the popup was
 * promoted from touch or pen input (MOUSEEVENTF_FROMTOUCH signature). Valid
 * inside the mouse callback; consumers pick touch behaviors with it.
 */
bool         flux_popup_lasttouch(FluxPopup const *popup);

/**
 * @brief Whether the popup window currently has a host-backdrop acrylic
 * backplate behind its content (composition mode only).
 *
 * Chrome painters should draw their background as a translucent tint when this
 * is true, so the blurred desktop behind the window shows through.
 */
bool         flux_popup_acrylic_active(FluxPopup const *popup);

/**
 * @brief Tint a chrome fill for acrylic: returns @p fill scaled to a translucent
 * tint when the popup's acrylic backplate is active, otherwise @p fill unchanged.
 *
 * Lets chrome painters stay agnostic — call it on the background color before
 * filling, and the blurred backdrop shows through only when acrylic is on.
 */
FluxColor    flux_popup_acrylic_tint(FluxPopup const *popup, FluxColor fill);

/**
 * @brief Set the maximum permitted popup content height in DIPs. The popup
 * will clamp its own size to this value at show time / position update,
 * which is the safest way to ensure the card never exceeds the monitor
 * work area. Pass 0 to disable the clamp (default).
 */
void         flux_popup_setmaxh(FluxPopup *popup, float max_h);

/**
 * @brief Dismiss every visible popup whose owner HWND matches `owner_hwnd`.
 */
void         flux_popup_dismissowner(HWND owner_hwnd);

/**
 * @brief True if any visible light-dismiss popup (dismiss-on-outside, e.g. a flyout,
 * menu, or ComboBox drop-down) is open for `owner_hwnd`. Tooltips are excluded.
 */
bool         flux_popup_lightdismiss_visible(HWND owner_hwnd);


#endif
