/**
 * @file flux_teaching_tip_data.h
 * @brief Data structures for the FluxTeachingTip control (WinUI 3 TeachingTip).
 */
#ifndef FLUX_TEACHING_TIP_DATA_H
#define FLUX_TEACHING_TIP_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef XtkTeachingTipPlacement   FluxTeachingTipPlacement;
typedef XtkTeachingTipCloseReason FluxTeachingTipCloseReason;

#define FLUX_TIP_PLACE_AUTO         XTK_TIP_PLACE_AUTO
#define FLUX_TIP_PLACE_TOP          XTK_TIP_PLACE_TOP
#define FLUX_TIP_PLACE_BOTTOM       XTK_TIP_PLACE_BOTTOM
#define FLUX_TIP_PLACE_LEFT         XTK_TIP_PLACE_LEFT
#define FLUX_TIP_PLACE_RIGHT        XTK_TIP_PLACE_RIGHT
#define FLUX_TIP_PLACE_TOP_RIGHT    XTK_TIP_PLACE_TOP_RIGHT
#define FLUX_TIP_PLACE_TOP_LEFT     XTK_TIP_PLACE_TOP_LEFT
#define FLUX_TIP_PLACE_BOTTOM_RIGHT XTK_TIP_PLACE_BOTTOM_RIGHT
#define FLUX_TIP_PLACE_BOTTOM_LEFT  XTK_TIP_PLACE_BOTTOM_LEFT
#define FLUX_TIP_PLACE_LEFT_TOP     XTK_TIP_PLACE_LEFT_TOP
#define FLUX_TIP_PLACE_LEFT_BOTTOM  XTK_TIP_PLACE_LEFT_BOTTOM
#define FLUX_TIP_PLACE_RIGHT_TOP    XTK_TIP_PLACE_RIGHT_TOP
#define FLUX_TIP_PLACE_RIGHT_BOTTOM XTK_TIP_PLACE_RIGHT_BOTTOM
#define FLUX_TIP_PLACE_CENTER       XTK_TIP_PLACE_CENTER

#define FLUX_TIP_CLOSE_BUTTON        XTK_TIP_CLOSE_BUTTON
#define FLUX_TIP_CLOSE_LIGHT_DISMISS XTK_TIP_CLOSE_LIGHT_DISMISS
#define FLUX_TIP_CLOSE_PROGRAMMATIC  XTK_TIP_CLOSE_PROGRAMMATIC

/** @brief TailVisibility: Auto = tail iff targeted, Visible = always, Collapsed = never. */
#define FLUX_TIP_TAIL_AUTO      0
#define FLUX_TIP_TAIL_VISIBLE   1
#define FLUX_TIP_TAIL_COLLAPSED 2

/**
 * @brief Model for a teaching tip: a FluxPopup-hosted card with title, subtitle,
 * optional icon, action/close buttons and a pointer tail anchored to a target.
 *
 * The control owns the strings: creation deep-copies the caller's strings and
 * the destructor frees them. @ref effective is the placement chosen by the
 * solver at open; FLUX_TIP_PLACE_AUTO while closed (or untargeted, no tail).
 */
typedef struct FluxTeachingTipData {
	char const              *title;         /**< Owned copy; NULL/"" hides the title row. */
	char const              *subtitle;      /**< Owned copy; NULL/"" hides the subtitle row. */
	uint32_t                 icon_glyph;    /**< Segoe Fluent Icons codepoint; 0 = no icon. */
	char const              *action_text;   /**< Owned copy; NULL omits the action button. */
	bool                     action_accent; /**< Accent-style the action button. */
	char const              *close_text;    /**< Owned copy; NULL = header X instead of footer Close. */
	FluxTeachingTipPlacement preferred;     /**< Preferred placement; AUTO picks per fallback order. */
	FluxTeachingTipPlacement effective;     /**< Solved placement while open; AUTO otherwise. */
	int8_t                   tail_visibility; /**< FLUX_TIP_TAIL_*. */
	bool                     light_dismiss; /**< Outside press dismisses; acrylic background; no X. */
	bool                     untargeted;    /**< Window-relative (24px edge margin), no anchor. */
	float                    placement_margin; /**< Extra gap between target/window edge and tip. */
	bool                     open;          /**< Whether the tip is showing. */
	void                     (*on_action)(void *ctx);  /**< Action button click (tip stays open). */
	void                     (*on_close)(void *ctx, FluxTeachingTipCloseReason reason);
	void                    *userdata;
} FluxTeachingTipData;

/* -------------------------------------------------------------------------
 * Popup-painter contract (behavior layout -> draw chrome). The behavior file
 * computes the geometry once per open; the popup paint callback hands it to
 * flux_teaching_tip_paint_surface each frame.
 * ---------------------------------------------------------------------- */

/** @brief Interactive zones inside the tip surface. */
#define FLUX_TIP_ZONE_NONE   0
#define FLUX_TIP_ZONE_ACTION 1 /**< Footer action button. */
#define FLUX_TIP_ZONE_CLOSE  2 /**< Footer close button. */
#define FLUX_TIP_ZONE_X      3 /**< Header 40x40 "X" close button. */

/** @brief Card edge the tail protrudes from. */
#define FLUX_TIP_TAIL_EDGE_NONE   0
#define FLUX_TIP_TAIL_EDGE_TOP    1
#define FLUX_TIP_TAIL_EDGE_BOTTOM 2
#define FLUX_TIP_TAIL_EDGE_LEFT   3
#define FLUX_TIP_TAIL_EDGE_RIGHT  4

typedef struct FluxRenderContext FluxRenderContext;
typedef struct FluxTextRenderer  FluxTextRenderer;

/**
 * @brief Everything flux_teaching_tip_paint_surface needs to draw one frame.
 *
 * All coordinates are popup-local DIPs; the popup content is the WinUI 5x5
 * tail-occlusion grid: the card inset by an 8px tail band on every side.
 */
typedef struct FluxTeachingTipPaintInfo {
	FluxTeachingTipData const *model;
	float                      tip_w, tip_h;       /**< Full popup content incl. 8px tail bands. */
	uint8_t                    tail_edge;          /**< FLUX_TIP_TAIL_EDGE_*. */
	float                      tail_center;        /**< Tail center along that edge (x for top/bottom, y for left/right). */
	FluxColor                  background;         /**< Resolved card + tail fill (acrylic-tinted upstream). */
	float                      scale_x, scale_y;   /**< Expand/contract animation scale (1,1 when idle). */
	float                      scale_cx, scale_cy; /**< Scale center = the tail tip. */
	int                        hot_zone;           /**< FLUX_TIP_ZONE_* under the pointer. */
	int                        pressed_zone;       /**< FLUX_TIP_ZONE_* under an active press. */
	FluxRect                   action_rect;        /**< Footer action button; w == 0 -> hidden. */
	FluxRect                   close_rect;         /**< Footer close button; w == 0 -> hidden. */
	FluxRect                   x_rect;             /**< Header X button; w == 0 -> hidden. */
} FluxTeachingTipPaintInfo;

/** @brief Draw the whole tip surface (card, tail, highlight, texts, buttons). */
void  flux_teaching_tip_paint_surface(FluxRenderContext const *rc, FluxTeachingTipPaintInfo const *info);

/** @brief Wrapped height of a title/subtitle block at @p max_w (14px, title = SemiBold). */
float flux_teaching_tip_text_height(FluxTextRenderer *text, char const *s, bool title, float max_w);

#ifdef __cplusplus
}
#endif

#endif
