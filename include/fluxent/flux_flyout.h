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

#define FLUX_FLYOUT_PAD_LEFT      16.0f
#define FLUX_FLYOUT_PAD_TOP       15.0f
#define FLUX_FLYOUT_PAD_RIGHT     16.0f
#define FLUX_FLYOUT_PAD_BOTTOM    17.0f
#define FLUX_FLYOUT_CORNER_RADIUS 8.0f
#define FLUX_FLYOUT_BORDER_WIDTH  1.0f
#define FLUX_FLYOUT_OFFSET        4.0f

/** @brief Opaque flyout handle. */
typedef struct FluxFlyout FluxFlyout;

/**
 * @brief Paint callback for flyout content.
 */
typedef void              (*FluxFlyoutPaintCallback)(void *user_ctx, FluxFlyout *flyout, FluxRect const *content_rect);

/**
 * @brief Create a flyout owned by @p owner.
 * The flyout creates an internal FluxPopup with dismiss_on_outside = true.
 */
FluxFlyout               *flux_flyout_create(FluxWindow *owner);

/**
 * @brief Destroy the flyout and release all resources.
 */
void                      flux_flyout_destroy(FluxFlyout *flyout);

/**
 * @brief Set the shared text renderer used to measure or draw
 * text inside the paint callback via the render context helper).
 */
void                      flux_flyout_set_text_renderer(FluxFlyout *flyout, FluxTextRenderer *tr);

/**
 * @brief Update theme colours.
 * @deprecated Use flux_flyout_set_theme_manager() instead for automatic theme updates.
 */
void                      flux_flyout_set_theme(FluxFlyout *flyout, FluxThemeColors const *theme, bool is_dark);

/**
 * @brief Set the theme manager for automatic theme synchronization.
 * The flyout will query the manager on each show/paint to stay in sync
 * with theme changes (including system dark/light mode switches).
 * Pass NULL to disable automatic theme management.
 */
void                      flux_flyout_set_theme_manager(FluxFlyout *flyout, FluxThemeManager *tm);

/**
 * @brief Set the desired content size, excluding padding and border.
 * The flyout popup will be enlarged by the padding and border automatically.
 */
void                      flux_flyout_set_content_size(FluxFlyout *flyout, float width, float height);

/**
 * @brief Register a callback that renders the flyout's interior content.
 */
void          flux_flyout_set_paint_callback(FluxFlyout *flyout, FluxFlyoutPaintCallback cb, void *user_ctx);

/**
 * @brief Register a callback that fires when the flyout is dismissed.
 */
void          flux_flyout_set_dismiss_callback(FluxFlyout *flyout, FluxPopupDismissCallback cb, void *user_ctx);

/**
 * @brief Show the flyout anchored to @p anchor in screen coordinates with the
 * given placement preference.  The underlying popup handles flip logic
 * when the flyout would otherwise go off-screen.
 */
void          flux_flyout_show(FluxFlyout *flyout, FluxRect anchor, FluxPlacement placement);

/**
 * @brief Programmatically dismiss the flyout.
 */
void          flux_flyout_dismiss(FluxFlyout *flyout);

/**
 * @brief Returns true while the flyout popup is visible.
 */
bool          flux_flyout_is_visible(FluxFlyout const *flyout);

/**
 * @brief Get the popup's FluxGraphics handle.
 */
FluxGraphics *flux_flyout_get_graphics(FluxFlyout *flyout);

/**
 * @brief Get the flyout's owner window.
 */
FluxWindow   *flux_flyout_get_owner(FluxFlyout *flyout);

/**
 * @brief Get the underlying FluxPopup.
 */
FluxPopup    *flux_flyout_get_popup(FluxFlyout *flyout);

/**
 * @brief Get the theme colours currently set on this flyout.
 */
FluxThemeColors const *flux_flyout_get_theme(FluxFlyout const *flyout);

/**
 * @brief Query whether the flyout is in dark mode.
 */
bool                   flux_flyout_is_dark(FluxFlyout const *flyout);

/**
 * @brief Get the text renderer associated with this flyout.
 */
FluxTextRenderer      *flux_flyout_get_text_renderer(FluxFlyout *flyout);

#ifdef __cplusplus
}
#endif

#endif
