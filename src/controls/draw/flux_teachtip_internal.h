/**
 * @file flux_teachtip_internal.h
 * @brief Private display-list contract for TeachingTip popup chrome.
 */
#ifndef FLUX_TEACHTIP_INTERNAL_H
#define FLUX_TEACHTIP_INTERNAL_H

#include "controls/data/flux_teaching_tip_data.h"

#include <stdbool.h>
#include <stdint.h>


/** @brief Interactive zones inside the tip surface. */
#define FLUX_TIP_ZONE_NONE        0
#define FLUX_TIP_ZONE_ACTION      1 /**< Footer action button. */
#define FLUX_TIP_ZONE_CLOSE       2 /**< Footer close button. */
#define FLUX_TIP_ZONE_X           3 /**< Header 40x40 "X" close button. */

/** @brief Card edge the tail protrudes from. */
#define FLUX_TIP_TAIL_EDGE_NONE   0
#define FLUX_TIP_TAIL_EDGE_TOP    1
#define FLUX_TIP_TAIL_EDGE_BOTTOM 2
#define FLUX_TIP_TAIL_EDGE_LEFT   3
#define FLUX_TIP_TAIL_EDGE_RIGHT  4

typedef struct FluxRenderCtx FluxRenderCtx;
typedef struct FluxText      FluxText;

/**
 * @brief Everything flux_teachtip_record_surface needs to record one frame.
 *
 * All coordinates are popup-local DIPs; the popup content is the WinUI 5x5
 * tail-occlusion grid: the card inset by an 8px tail band on every side.
 */
typedef struct FluxTeachingTipPaintInfo {
	FluxTeachingTipData const *model;
	float                      tip_w, tip_h; /**< Full popup content incl. 8px tail bands. */
	uint8_t                    tail_edge;    /**< FLUX_TIP_TAIL_EDGE_*. */
	float                      tail_center;  /**< Tail center along that edge (x for top/bottom, y for left/right). */
	FluxColor                  background;   /**< Resolved card + tail fill (acrylic-tinted upstream). */
	float                      scale_x, scale_y;   /**< Expand/contract animation scale (1,1 when idle). */
	float                      scale_cx, scale_cy; /**< Scale center = the tail tip. */
	int                        hot_zone;           /**< FLUX_TIP_ZONE_* under the pointer. */
	int                        pressed_zone;       /**< FLUX_TIP_ZONE_* under an active press. */
	FluxRect                   action_rect;        /**< Footer action button; w == 0 -> hidden. */
	FluxRect                   close_rect;         /**< Footer close button; w == 0 -> hidden. */
	FluxRect                   x_rect;             /**< Header X button; w == 0 -> hidden. */
} FluxTeachingTipPaintInfo;

XentDl *flux_teachtip_record_surface(FluxRenderCtx const *rc, FluxTeachingTipPaintInfo const *info);

/** @brief Wrapped height of a title/subtitle block at @p max_w (14px, title = SemiBold). */
float   flux_teachtip_text_height(FluxText *text, char const *s, bool title, float max_w);


#endif
