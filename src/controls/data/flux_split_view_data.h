#ifndef FLUX_SPLIT_VIEW_DATA_H
#define FLUX_SPLIT_VIEW_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

typedef struct FluxStore FluxStore;

typedef enum FluxSplitViewDisplayMode
{
	FLUX_SPLITVIEW_INLINE = 0,
	FLUX_SPLITVIEW_COMPACT_INLINE,
	FLUX_SPLITVIEW_OVERLAY,
	FLUX_SPLITVIEW_COMPACT_OVERLAY,
} FluxSplitViewDisplayMode;

typedef enum FluxSplitViewPanePlacement
{
	FLUX_SPLITVIEW_LEFT = 0,
	FLUX_SPLITVIEW_RIGHT,
} FluxSplitViewPanePlacement;

#define FLUX_SPLITVIEW_OPEN_LEN    320.0f
#define FLUX_SPLITVIEW_COMPACT_LEN 48.0f

typedef struct FluxSplitViewData {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId node;

	uint8_t    display_mode;
	uint8_t    placement;
	bool       pane_open;
	float      open_len;
	float      compact_len;
} FluxSplitViewData;

typedef struct FluxSplitPaneData {
	bool    overlay;
	uint8_t placement;
	bool    divider;
} FluxSplitPaneData;

static bool inline flux_splitview_is_compact(int mode) {
	return mode == FLUX_SPLITVIEW_COMPACT_INLINE || mode == FLUX_SPLITVIEW_COMPACT_OVERLAY;
}

static bool inline flux_splitview_is_overlay(int mode) {
	return mode == FLUX_SPLITVIEW_OVERLAY || mode == FLUX_SPLITVIEW_COMPACT_OVERLAY;
}

#endif
