/**
 * @file flux_scroll_data.h
 * @brief Data structure for FluxScrollView control.
 */
#ifndef FLUX_SCROLL_DATA_H
#define FLUX_SCROLL_DATA_H

#include "../flux_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════════════════════════
   Scrollbar visibility enum
   ═══════════════════════════════════════════════════════════════════════ */

typedef enum FluxScrollBarVis
{
	FLUX_SCROLL_AUTO,   /**< Show scrollbar only when needed */
	FLUX_SCROLL_ALWAYS, /**< Always show scrollbar */
	FLUX_SCROLL_NEVER,  /**< Never show scrollbar (can still scroll) */
} FluxScrollBarVis;

/* ═══════════════════════════════════════════════════════════════════════
   FluxScrollData — Control-specific data for FluxScrollView
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration and runtime state for a scrollable container.
 *
 * Combines configuration (scroll position, content size, visibility) with
 * runtime interaction state (hover, drag, button presses) needed for
 * rendering scrollbar visuals that match WinUI's ScrollBar template.
 */
typedef struct FluxScrollData {
	/* -------- Configuration (set by user) -------- */
	float            scroll_x;  /**< Current horizontal scroll offset */
	float            scroll_y;  /**< Current vertical scroll offset */
	float            content_w; /**< Total content width */
	float            content_h; /**< Total content height */
	FluxScrollBarVis h_vis;     /**< Horizontal scrollbar visibility */
	FluxScrollBarVis v_vis;     /**< Vertical scrollbar visibility */

	/* -------- Runtime interaction state --------
	 * Updated by flux_input.c, read by flux_draw_scroll_overlay.
	 * Do NOT use these fields to drive scroll offset; they exist purely so
	 * the scrollbar renderer can show hover/press/drag visuals that match
	 * WinUI's ScrollBar template (LineUp/LineDown RepeatButton + Thumb). */
	uint8_t          mouse_over;         /**< 1 if cursor inside viewport */
	float            mouse_local_x;      /**< Cursor X in viewport-local coords */
	float            mouse_local_y;      /**< Cursor Y in viewport-local coords */
	uint8_t          drag_axis;          /**< 0=none, 1=vertical drag, 2=horizontal */
	uint8_t          v_up_pressed;       /**< Vertical LineUp arrow held */
	uint8_t          v_dn_pressed;       /**< Vertical LineDown arrow held */
	uint8_t          h_lf_pressed;       /**< Horizontal LineLeft arrow held */
	uint8_t          h_rg_pressed;       /**< Horizontal LineRight arrow held */
	double           last_activity_time; /**< Monotonic seconds (0 = never) */

	/* -------- DirectManipulation --------
	 * Viewport for this ScrollView. Lazily created by FluxApp's render
	 * walk the first time layout yields a non-empty rect.
	 * Typed as void* to avoid pulling DManip headers here. */
	void            *dmanip_viewport; /**< IDirectManipulationViewport (opaque) */
} FluxScrollData;

#ifdef __cplusplus
}
#endif

#endif /* FLUX_SCROLL_DATA_H */
