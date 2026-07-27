/**
 * @file flux_teachtip_data.h
 * @brief Data structures for the FluxTeachingTip control (WinUI 3 TeachingTip).
 */
#ifndef FLUX_TEACHING_TIP_DATA_H
#define FLUX_TEACHING_TIP_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>


typedef XtkTeachingTipPlacement   FluxTeachingTipPlacement;
typedef XtkTeachingTipCloseReason FluxTeachingTipCloseReason;

#define FLUX_TIP_PLACE_AUTO          XTK_TIP_PLACE_AUTO
#define FLUX_TIP_PLACE_TOP           XTK_TIP_PLACE_TOP
#define FLUX_TIP_PLACE_BOTTOM        XTK_TIP_PLACE_BOTTOM
#define FLUX_TIP_PLACE_LEFT          XTK_TIP_PLACE_LEFT
#define FLUX_TIP_PLACE_RIGHT         XTK_TIP_PLACE_RIGHT
#define FLUX_TIP_PLACE_TOP_RIGHT     XTK_TIP_PLACE_TOP_RIGHT
#define FLUX_TIP_PLACE_TOP_LEFT      XTK_TIP_PLACE_TOP_LEFT
#define FLUX_TIP_PLACE_BOTTOM_RIGHT  XTK_TIP_PLACE_BOTTOM_RIGHT
#define FLUX_TIP_PLACE_BOTTOM_LEFT   XTK_TIP_PLACE_BOTTOM_LEFT
#define FLUX_TIP_PLACE_LEFT_TOP      XTK_TIP_PLACE_LEFT_TOP
#define FLUX_TIP_PLACE_LEFT_BOTTOM   XTK_TIP_PLACE_LEFT_BOTTOM
#define FLUX_TIP_PLACE_RIGHT_TOP     XTK_TIP_PLACE_RIGHT_TOP
#define FLUX_TIP_PLACE_RIGHT_BOTTOM  XTK_TIP_PLACE_RIGHT_BOTTOM
#define FLUX_TIP_PLACE_CENTER        XTK_TIP_PLACE_CENTER

#define FLUX_TIP_CLOSE_BUTTON        XTK_TIP_CLOSE_BUTTON
#define FLUX_TIP_CLOSE_LIGHT_DISMISS XTK_TIP_CLOSE_LIGHT_DISMISS
#define FLUX_TIP_CLOSE_PROGRAMMATIC  XTK_TIP_CLOSE_PROGRAMMATIC

/** @brief TailVisibility: Auto = tail iff targeted, Visible = always, Collapsed = never. */
#define FLUX_TIP_TAIL_AUTO           0
#define FLUX_TIP_TAIL_VISIBLE        1
#define FLUX_TIP_TAIL_COLLAPSED      2

/**
 * @brief Model for a teaching tip: a FluxPopup-hosted card with title, subtitle,
 * optional icon, action/close buttons and a pointer tail anchored to a target.
 *
 * The control owns the strings: creation deep-copies the caller's strings and
 * the destructor frees them. @ref effective is the placement chosen by the
 * solver at open; FLUX_TIP_PLACE_AUTO while closed (or untargeted, no tail).
 */
typedef struct FluxTeachingTipData {
	char const              *title;                   /**< Owned copy; NULL/"" hides the title row. */
	char const              *subtitle;                /**< Owned copy; NULL/"" hides the subtitle row. */
	uint32_t                 icon_glyph;              /**< Segoe Fluent Icons codepoint; 0 = no icon. */
	char const              *action_text;             /**< Owned copy; NULL omits the action button. */
	bool                     action_accent;           /**< Accent-style the action button. */
	char const              *close_text;              /**< Owned copy; NULL = header X instead of footer Close. */
	FluxTeachingTipPlacement preferred;               /**< Preferred placement; AUTO picks per fallback order. */
	FluxTeachingTipPlacement effective;               /**< Solved placement while open; AUTO otherwise. */
	int8_t                   tail_visibility;         /**< FLUX_TIP_TAIL_*. */
	bool                     light_dismiss;           /**< Outside press dismisses; acrylic background; no X. */
	bool                     untargeted;              /**< Window-relative (24px edge margin), no anchor. */
	float                    placement_margin;        /**< Extra gap between target/window edge and tip. */
	bool                     open;                    /**< Whether the tip is showing. */
	void                     (*on_action)(void *ctx); /**< Action button click (tip stays open). */
	void                    *action_ctx;
	void                     (*on_close)(void *ctx, FluxTeachingTipCloseReason reason);
	void                    *close_ctx;
} FluxTeachingTipData;


#endif
