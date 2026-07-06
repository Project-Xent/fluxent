/**
 * @file flux_split_view_data.h
 * @brief Data for FluxSplitView — a collapsible side pane (WinUI 3 SplitView).
 *
 * The root (FLUX_CONTROL_SPLIT_VIEW) is an ABSOLUTE host of two internal
 * wrappers created by the declarative builder: a content wrapper (drawn first)
 * and a pane wrapper (FLUX_CONTROL_SPLIT_VIEW_PANE, drawn on top for the overlay
 * modes). A per-frame sync positions the two wrappers per display mode /
 * placement / open state; the pane wrapper's own data tells its renderer whether
 * to draw the floating (overlay) surface and which edge carries the divider.
 */
#ifndef FLUX_SPLIT_VIEW_DATA_H
#define FLUX_SPLIT_VIEW_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxNodeStore FluxNodeStore;

/** @brief Display mode; values match XtkSplitViewDisplayMode. */
typedef enum FluxSplitViewDisplayMode
{
	FLUX_SPLITVIEW_INLINE = 0,
	FLUX_SPLITVIEW_COMPACT_INLINE,
	FLUX_SPLITVIEW_OVERLAY,
	FLUX_SPLITVIEW_COMPACT_OVERLAY,
} FluxSplitViewDisplayMode;

/** @brief Pane placement; values match XtkSplitViewPanePlacement. */
typedef enum FluxSplitViewPanePlacement
{
	FLUX_SPLITVIEW_LEFT = 0,
	FLUX_SPLITVIEW_RIGHT,
} FluxSplitViewPanePlacement;

#define FLUX_SPLITVIEW_OPEN_LEN    320.0f /**< Default OpenPaneLength. */
#define FLUX_SPLITVIEW_COMPACT_LEN 48.0f  /**< Default CompactPaneLength. */

/** @brief Retained state for a FLUX_CONTROL_SPLIT_VIEW root. */
typedef struct FluxSplitViewData {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     node;

	uint8_t        display_mode; /**< FluxSplitViewDisplayMode. */
	uint8_t        placement;    /**< FluxSplitViewPanePlacement. */
	bool           pane_open;
	float          open_len;     /**< OpenPaneLength (DIP). */
	float          compact_len;  /**< CompactPaneLength (DIP). */
} FluxSplitViewData;

/** @brief Retained state for a FLUX_CONTROL_SPLIT_VIEW_PANE wrapper. */
typedef struct FluxSplitPaneData {
	bool    overlay;   /**< Floating (overlay) surface: opaque background + shadow. */
	uint8_t placement; /**< FluxSplitViewPanePlacement (divider edge). */
	bool    divider;   /**< Draw the 1px divider on the content-facing edge. */
} FluxSplitPaneData;

static bool inline flux_splitview_is_compact(int mode) {
	return mode == FLUX_SPLITVIEW_COMPACT_INLINE || mode == FLUX_SPLITVIEW_COMPACT_OVERLAY;
}

static bool inline flux_splitview_is_overlay(int mode) {
	return mode == FLUX_SPLITVIEW_OVERLAY || mode == FLUX_SPLITVIEW_COMPACT_OVERLAY;
}

#ifdef __cplusplus
}
#endif

#endif
