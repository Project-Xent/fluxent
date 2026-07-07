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

/** @brief Scrollbar visibility policy for each axis. */
typedef enum FluxScrollBarVis
{
	FLUX_SCROLL_AUTO,   /**< Show scrollbar only when needed */
	FLUX_SCROLL_ALWAYS, /**< Always show scrollbar */
	FLUX_SCROLL_NEVER,  /**< Never show scrollbar (can still scroll) */
} FluxScrollBarVis;

/**
 * @brief Configuration and runtime state for a scrollable container.
 *
 * Combines configuration (scroll position, content size, visibility) with
 * runtime interaction state (hover, drag, button presses) needed for
 * rendering scrollbar visuals that match WinUI's ScrollBar template.
 */
typedef struct FluxScrollData {
	float            scroll_x;  /**< Current horizontal scroll offset */
	float            scroll_y;  /**< Current vertical scroll offset */
	float            content_w; /**< Total content width */
	float            content_h; /**< Total content height */
	FluxScrollBarVis h_vis;     /**< Horizontal scrollbar visibility */
	FluxScrollBarVis v_vis;     /**< Vertical scrollbar visibility */

	/**
	 * Content extent ownership. 0 (default): the engine derives content_w/h
	 * from the laid-out children each frame, so pages never hand-compute
	 * their own height. 1: the owner writes content_w/h itself (e.g. the
	 * TabView strip); the engine leaves them alone.
	 */
	uint8_t          content_manual;

	/**
	 * Virtualized items host. The logical position lives only in scroll_x/y +
	 * content_w/h (scrollbar, clamping, window math); child layout stays
	 * rebased near the origin so the compositor / D2D / hit paths never see
	 * deep-scroll coordinates (float32 breaks down at ~4e6). Consumers that
	 * walk physical child coordinates must use flux_scroll_off_x/y.
	 */
	uint8_t          virtualized;
	float            origin_x;           /**< Logical X mapped to physical child x=0 (0 = identity). */
	float            origin_y;           /**< Logical Y mapped to physical child y=0 (0 = identity). */

	uint8_t          mouse_over;         /**< 1 if cursor inside viewport */
	float            mouse_local_x;      /**< Cursor X in viewport-local coords */
	float            mouse_local_y;      /**< Cursor Y in viewport-local coords */
	uint8_t          drag_axis;          /**< 0=none, 1=vertical drag, 2=horizontal */
	uint8_t          v_up_pressed;       /**< Vertical LineUp arrow held */
	uint8_t          v_dn_pressed;       /**< Vertical LineDown arrow held */
	uint8_t          h_lf_pressed;       /**< Horizontal LineLeft arrow held */
	uint8_t          h_rg_pressed;       /**< Horizontal LineRight arrow held */
	double           last_activity_time; /**< Monotonic seconds (0 = never) */

	void            *dmanip_viewport;    /**< Opaque DirectManipulation viewport owned by the input boundary. */
} FluxScrollData;

/** @brief Residual render offset: scroll position rebased against the physical origin. */
static inline float flux_scroll_off_x(FluxScrollData const *sd) { return sd->scroll_x - sd->origin_x; }
static inline float flux_scroll_off_y(FluxScrollData const *sd) { return sd->scroll_y - sd->origin_y; }

#ifdef __cplusplus
}
#endif

#endif
