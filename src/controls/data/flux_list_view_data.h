#ifndef FLUX_LIST_VIEW_DATA_H
#define FLUX_LIST_VIEW_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <xent/xent.h>
#include <xtk/xtk.h>

typedef struct FluxInput FluxInput;

typedef struct FluxListSelRange {
	int first;
	int last;
} FluxListSelRange;

typedef struct FluxSizeRing {
	float *slots;
	double total;
	int    stored;
} FluxSizeRing;

typedef struct FluxListViewData {
	XentCtx          *ctx;
	FluxStore        *store;
	FluxInput        *input;
	XentNodeId        root;
	XentNodeId        scroll;
	XentNodeId        host;
	XentNodeId        header;
	float             header_h;
	float             header_w;

	XtkListKind       kind;
	XtkListSelMode    sel_mode;

	int               count;
	float             item_height;
	float             item_width;
	int               columns;
	int               cols;

	int               selected;

	int               realized_first;
	int               realized_last;
	float             block_top;
	float             block_bottom;
	int               pin;
	float             pin_y;
	bool              pinned;

	FluxSizeRing      item_sizes;
	FluxSizeRing      header_sizes;

	int              *groups;
	int              *group_row;
	int              *group_item;
	int              *group_line;
	int               ngroup;

	char             *names;
	uint32_t         *name_at;
	int               nname;

	bool              rect_select;
	uint8_t           band;
	uint8_t           band_mode;
	float             band_x;
	float             band_y;
	float             band_cx;
	float             band_cy;
	FluxListSelRange *band_prev;
	int               band_prev_count;
	bool              stale_fired;
	bool              realize_events;

	int               focused;
	int               pending_focus;
	int               anchor;
	bool              anchor_set;
	bool              single_follows_focus;

	FluxListSelRange *ranges;
	int               range_count;
	int               range_cap;

	void              (*on_select)(void *ctx, int index);
	void             *on_select_ctx;

	bool              item_invoked_enabled;
	void              (*on_invoke)(void *ctx, int index);
	void             *on_invoke_ctx;

	void              (*on_stale)(void *ctx);
	void             *on_stale_ctx;
} FluxListViewData;

typedef struct FluxListItemData {
	FluxListViewData *owner;
	int               index;
	int               row;
	int               line;
	int               col;
	int               group;
	bool              header;
	float             logical_x;
	float             logical_y;
	float             extent;
	bool              selected;
	bool              multi;
} FluxListItemData;

#endif
