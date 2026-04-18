/**
 * @file flux_flyout.h
 * @brief Flyout popup control (WinUI 3 Flyout equivalent).
 *
 * Displays transient content anchored to a target element. Supports
 * light-dismiss and keyboard navigation.
 */

#ifndef FLUX_FLYOUT_H
#define FLUX_FLYOUT_H

#include "flux_types.h"
#include "flux_popup.h"
#include "flux_text.h"
#include "flux_theme.h"
#include "flux_window.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════════════════════
 *  WinUI 3 FlyoutPresenter constants
 *  Source: CommonStyles/FlyoutPresenter_themeresources.xaml
 * ═══════════════════════════════════════════════════════════════════ */

#define FLUX_FLYOUT_PAD_LEFT      16.0f
#define FLUX_FLYOUT_PAD_TOP       15.0f
#define FLUX_FLYOUT_PAD_RIGHT     16.0f
#define FLUX_FLYOUT_PAD_BOTTOM    17.0f
#define FLUX_FLYOUT_CORNER_RADIUS 8.0f /* OverlayCornerRadius   */
#define FLUX_FLYOUT_BORDER_WIDTH  1.0f
#define FLUX_FLYOUT_OFFSET        4.0f /* gap between anchor and popup */

/* ── Opaque handle ──────────────────────────────────────────────── */

typedef struct FluxFlyout FluxFlyout;

/* ── Paint callback ─────────────────────────────────────────────── */

/**
 * Called during WM_PAINT after the flyout card (background + border) has
 * been drawn.  The callee should render its content inside @p content_rect,
 * which is the padded interior of the flyout card.
 *
 * To obtain the D2D device context for drawing, use:
 *   FluxGraphics *gfx = flux_flyout_get_graphics(flyout);
 *   ID2D1DeviceContext *d2d = flux_graphics_get_d2d_context(gfx);
 *
 * @param user_ctx      User-provided context pointer.
 * @param flyout        The flyout being painted.
 * @param content_rect  Padded content area in logical coordinates.
 */
typedef void              (*FluxFlyoutPaintCallback)(void *user_ctx, FluxFlyout *flyout, FluxRect const *content_rect);

/* ── Lifecycle ──────────────────────────────────────────────────── */

/**
 * Create a flyout owned by @p owner.
 * The flyout creates an internal FluxPopup with dismiss_on_outside = true.
 */
FluxFlyout               *flux_flyout_create(FluxWindow *owner);

/**
 * Destroy the flyout and release all resources.
 */
void                      flux_flyout_destroy(FluxFlyout *flyout);

/* ── Configuration ──────────────────────────────────────────────── */

/**
 * Set the shared text renderer (used if the caller wants to measure/draw
 * text inside the paint callback via the render context helper).
 */
void                      flux_flyout_set_text_renderer(FluxFlyout *flyout, FluxTextRenderer *tr);

/**
 * Update theme colours.  Call whenever the theme changes.
 */
void                      flux_flyout_set_theme(FluxFlyout *flyout, FluxThemeColors const *theme, bool is_dark);

/**
 * Set the desired *content* size (excluding padding and border).
 * The flyout popup will be enlarged by the padding and border automatically.
 */
void                      flux_flyout_set_content_size(FluxFlyout *flyout, float width, float height);

/**
 * Register a callback that renders the flyout's interior content.
 */
void          flux_flyout_set_paint_callback(FluxFlyout *flyout, FluxFlyoutPaintCallback cb, void *user_ctx);

/**
 * Register a callback that fires when the flyout is dismissed
 * (light-dismiss, Escape, or programmatic).
 */
void          flux_flyout_set_dismiss_callback(FluxFlyout *flyout, FluxPopupDismissCallback cb, void *user_ctx);

/* ── Show / Dismiss ─────────────────────────────────────────────── */

/**
 * Show the flyout anchored to @p anchor (screen coordinates) with the
 * given placement preference.  The underlying popup handles flip logic
 * when the flyout would otherwise go off-screen.
 */
void          flux_flyout_show(FluxFlyout *flyout, FluxRect anchor, FluxPlacement placement);

/**
 * Programmatically dismiss the flyout.
 */
void          flux_flyout_dismiss(FluxFlyout *flyout);

/**
 * Returns true while the flyout popup is visible.
 */
bool          flux_flyout_is_visible(FluxFlyout const *flyout);

/* ── Accessors (for use inside paint callbacks) ─────────────────── */

/**
 * Get the popup's FluxGraphics handle (gives access to the D2D context).
 */
FluxGraphics *flux_flyout_get_graphics(FluxFlyout *flyout);

/**
 * Get the flyout's owner window (useful for DPI queries etc.).
 */
FluxWindow   *flux_flyout_get_owner(FluxFlyout *flyout);

/**
 * Get the underlying FluxPopup (advanced use only).
 */
FluxPopup    *flux_flyout_get_popup(FluxFlyout *flyout);

/**
 * Get the theme colours currently set on this flyout.
 */
FluxThemeColors const *flux_flyout_get_theme(FluxFlyout const *flyout);

/**
 * Query whether the flyout is in dark mode.
 */
bool                   flux_flyout_is_dark(FluxFlyout const *flyout);

/**
 * Get the text renderer associated with this flyout.
 */
FluxTextRenderer      *flux_flyout_get_text_renderer(FluxFlyout *flyout);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_FLYOUT_H */
