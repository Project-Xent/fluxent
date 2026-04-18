/**
 * @file flux_tooltip.h
 * @brief Tooltip popup control (WinUI 3 ToolTip equivalent).
 *
 * Displays informational text when hovering over a target element.
 * Supports delayed show/hide and automatic repositioning.
 */

#ifndef FLUX_TOOLTIP_H
#define FLUX_TOOLTIP_H

#include "flux_types.h"
#include "flux_window.h"
#include "flux_popup.h"
#include "flux_text.h"
#include "flux_theme.h"
#include "flux_node_store.h"
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════════════════════
 *  Tooltip constants (matching WinUI 3 ToolTip_themeresources.xaml)
 * ═══════════════════════════════════════════════════════════════════ */

#define FLUX_TOOLTIP_DELAY_MS        800 /* initial hover delay          */
#define FLUX_TOOLTIP_RESHOW_DELAY_MS 0   /* delay when switching targets */
#define FLUX_TOOLTIP_MAX_WIDTH       320.0f
#define FLUX_TOOLTIP_FONT_SIZE       12.0f
#define FLUX_TOOLTIP_PAD_LEFT        9.0f
#define FLUX_TOOLTIP_PAD_TOP         6.0f
#define FLUX_TOOLTIP_PAD_RIGHT       9.0f
#define FLUX_TOOLTIP_PAD_BOTTOM      8.0f
#define FLUX_TOOLTIP_CORNER_RADIUS   4.0f
#define FLUX_TOOLTIP_BORDER_WIDTH    1.0f
#define FLUX_TOOLTIP_OFFSET          4.0f /* gap between anchor and popup */

/* ── Opaque handle ──────────────────────────────────────────────────── */

typedef struct FluxTooltip FluxTooltip;

/* ── Lifecycle ──────────────────────────────────────────────────────── */

/**
 * Create the app-level tooltip system.
 * Only one instance should exist per application.
 */
FluxTooltip               *flux_tooltip_create(FluxWindow *owner);

/**
 * Destroy the tooltip system and release all resources.
 */
void                       flux_tooltip_destroy(FluxTooltip *tt);

/* ── Configuration (call once after create, or when resources change) ─ */

/**
 * Set the shared text renderer used to measure and draw tooltip text.
 */
void                       flux_tooltip_set_text_renderer(FluxTooltip *tt, FluxTextRenderer *tr);

/**
 * Update theme colours used for tooltip rendering.
 * Call this whenever the theme changes.
 */
void                       flux_tooltip_set_theme(FluxTooltip *tt, FluxThemeColors const *theme, bool is_dark);

/* ── Runtime ────────────────────────────────────────────────────────── */

/**
 * Notify the tooltip system that the hover target changed.
 *
 * @param hovered   The node currently under the cursor
 *                  (XENT_NODE_INVALID if nothing is hovered).
 * @param nd        FluxNodeData for @p hovered (may be NULL).
 * @param screen_bounds  Layout rect of the hovered node in *screen*
 *                       coordinates (used for popup placement).
 *                       Ignored when hovered == XENT_NODE_INVALID.
 */
void flux_tooltip_on_hover(FluxTooltip *tt, XentNodeId hovered, FluxNodeData const *nd, FluxRect const *screen_bounds);

/**
 * Force-dismiss the current tooltip (e.g. on pointer-down or key press).
 */
void flux_tooltip_dismiss(FluxTooltip *tt);

/**
 * Returns true if the tooltip popup is currently visible.
 */
bool flux_tooltip_is_visible(FluxTooltip const *tt);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_TOOLTIP_H */
