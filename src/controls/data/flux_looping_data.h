#ifndef FLUX_LOOPING_DATA_H
#define FLUX_LOOPING_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <xent/xent.h>

typedef struct FluxInput FluxInput;

#define FLUX_LOOPING_PANEL_SLOTS 1001

#define FLUX_LOOPING_ITEM_HEIGHT 40.0f

#define FLUX_LOOPING_BUTTON_H    34.0f

typedef struct FluxLoopingData {
	XentCtx    *ctx;
	FluxStore  *store;
	FluxInput  *input;
	XentNodeId  root;
	XentNodeId  scroll;
	XentNodeId  host;

	char      **items;
	int         item_count;
	int         selected;

	float       item_height;
	float       item_width;
	bool        should_loop;
	bool        enabled;
	bool        accent;
	bool        left;

	XentNodeId *cells;
	int         cell_count;
	int         window_first;

	float       viewport_h;
	int         midpoint;

	bool        scrolling;

	bool        pointer_inside;
	bool        pointer_touch;

	bool        up_pressed;
	bool        down_pressed;

	void        (*on_select)(void *ctx, int index);
	void       *on_select_ctx;
} FluxLoopingData;

typedef struct FluxLoopingItemData {
	FluxLoopingData *owner;
	XentNodeId       node;
	int              visual;
	float            logical_y;
	float            band_y;
	bool             selected;
} FluxLoopingItemData;

#endif
