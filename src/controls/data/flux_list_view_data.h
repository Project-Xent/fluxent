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

typedef struct FluxListViewData {
	XentCtx          *ctx;
	FluxStore        *store;
	FluxInput        *input;
	XentNodeId        root;
	XentNodeId        scroll;
	XentNodeId        host;

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
	bool              stale_fired;

	bool              realize_pending;
	int               pending_first;
	int               pending_last;
	int               pending_cols;

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

	void              (*realize)(void *ctx, XtkCtrl list, int first, int last);
	void             *realize_ctx;
} FluxListViewData;

typedef struct FluxListItemData {
	FluxListViewData *owner;
	int               index;
	float             logical_x;
	float             logical_y;
	bool              selected;
	bool              multi;
} FluxListItemData;

#endif
