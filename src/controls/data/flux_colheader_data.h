#ifndef FLUX_COLHEADER_DATA_H
#define FLUX_COLHEADER_DATA_H

#include "fluxent/flux_node_store.h"

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

#define FLUX_COL_HEADER_H      40.0f
#define FLUX_COL_SPLITTER_W    12.0f
#define FLUX_COL_CELL_OVERLAP  4.0f
#define FLUX_COL_PAD_LEFT      12.0f
#define FLUX_COL_PAD_TOP       12.0f
#define FLUX_COL_TEXT_SIZE     12.0f
#define FLUX_COL_SORT_SIZE     8.0f
#define FLUX_COL_SORT_SECONDS  0.2f
#define FLUX_COL_REORDER_MS    150u
#define FLUX_COL_DRAG_MOUSE    6.0f
#define FLUX_COL_DRAG_TOUCH    16.0f
#define FLUX_COL_SCROLL_MARGIN 32.0f
#define FLUX_COL_SCROLL_STEP   16.0f
#define FLUX_COL_GHOST_OPACITY 0.85f
#define FLUX_COL_SPLIT_OPACITY 0.8f

typedef struct FluxWindow FluxWindow;

typedef enum FluxColPart
{
	FLUX_COL_PART_NONE = 0,
	FLUX_COL_PART_BUTTON,
	FLUX_COL_PART_SPLITTER,
} FluxColPart;

typedef struct FluxColItemData {
	XentCtx      *ctx;
	FluxStore    *store;
	XentNodeId    node;
	char const   *text;
	float         width;
	float         min_width;
	float         max_width;
	uint8_t       align;
	uint8_t       sort;
	bool          sortable;
	bool          resizable;
	uint8_t       press_part;
	uint8_t       focus_part;
	float         press_x;
	float         press_width;
	float         offset_from;
	float         offset_to;
	unsigned long offset_start;
	bool          offset_anim;
} FluxColItemData;

typedef struct FluxColHeaderData {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  node;
	FluxWindow *window;
	XentNodeId  drag;
	bool        reordering;
	bool        touch;
	float       press_x;
	float       ghost_left;
	float       ghost_x;
	float       ghost_w;
	int         target;
	bool        animating;
} FluxColHeaderData;

#endif
