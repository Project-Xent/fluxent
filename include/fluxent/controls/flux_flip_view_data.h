/**
 * @file flux_flip_view_data.h
 * @brief Data structures for FluxFlipView and FluxPipsPager.
 *
 * FlipView spine: root (clips children) → ABSOLUTE pages host. The xtk
 * children mount into the host; the engine sync pass sizes each page to
 * the viewport and lays them along the paging axis, and the host slides
 * to -selected × extent (animated for adjacent moves, snapped for jumps —
 * FlipView_Partial OnSelectedIndexChanged).
 *
 * PipsPager is a single leaf node: pips + nav carets are all drawn by the
 * renderer from this data; hits resolve against the same geometry.
 */
#ifndef FLUX_FLIP_VIEW_DATA_H
#define FLUX_FLIP_VIEW_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>
#include <xtk/xtk.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxWindow FluxWindow;

/** @brief Retained state for a FLUX_CONTROL_FLIP_VIEW root. */
typedef struct FluxFlipViewData {
	XentContext   *ctx;
	FluxNodeStore *store;
	FluxWindow    *window;    /**< Repaint target for slide-animation frames. */
	XentNodeId     root;
	XentNodeId     host;      /**< ABSOLUTE pages host (xtk children = pages). */

	int            selected;
	bool           vertical;

	float          offset;    /**< Current slide offset along the axis (px). */
	float          anim_from; /**< Slide tween start offset. */
	float          anim_to;   /**< Slide tween target offset. */
	unsigned long  anim_start;
	bool           anim;

	float          extent;    /**< Viewport length along the paging axis (last layout). */
	float          cross;     /**< Viewport length across it. */

	unsigned long  pointer_activity; /**< Tick of the last pointer move (3 s button fade). */
	unsigned long  last_wheel;       /**< Tick of the last wheel flip (200 ms gate). */
	int            wheel_sign;       /**< Direction of the last wheel delta. */
	uint8_t        pressed_btn;      /**< 0 none, 1 previous, 2 next (visual). */

	void (*on_select)(void *ctx, int index);
	void  *on_select_ctx;
} FluxFlipViewData;

/** @brief Retained state for a FLUX_CONTROL_PIPS_PAGER leaf. */
typedef struct FluxPipsPagerData {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     node;

	int            count;        /**< NumberOfPages. */
	int            selected;     /**< SelectedPageIndex. */
	int            max_visible;  /**< Visible pips cap (default 5). */
	bool           vertical;
	uint8_t        nav_vis;      /**< XtkPipsNavVis. */

	int            window_start; /**< First visible pip (selected kept centered). */
	int            pressed_pip;  /**< Pip index under an active press, or -1. */
	uint8_t        pressed_nav;  /**< 0 none, 1 previous, 2 next. */

	void (*on_select)(void *ctx, int index);
	void  *on_select_ctx;
} FluxPipsPagerData;

#ifdef __cplusplus
}
#endif

#endif
