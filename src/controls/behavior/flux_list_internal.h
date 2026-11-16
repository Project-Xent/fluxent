#ifndef FLUX_LIST_INTERNAL_H
#define FLUX_LIST_INTERNAL_H

#include "store/flux_node_data.h"
#include "controls/data/flux_list_view_data.h"
#include "controls/data/flux_scroll_data.h"

typedef struct FluxFlowCursor {
	int   line;
	float y;
	float h;
} FluxFlowCursor;

FluxScrollData   *flux_list_scroll(FluxListViewData const *ld);
FluxListItemData *flux_list_item(FluxListViewData const *ld, XentNodeId node);
XentNodeId        flux_list_next_row(FluxListViewData const *ld, XentNodeId child);
void              flux_list_layout_block(FluxListViewData *ld);
void              flux_list_release(FluxListViewData *ld);
void              flux_list_report(FluxListViewData *ld, int lead);
void              flux_list_band_enable(FluxListViewData *ld, bool on);

bool              flux_list_measured(FluxListViewData const *ld);
float             flux_list_pitch(FluxListViewData const *ld);
float             flux_list_row_y(FluxListViewData const *ld, int row);
float             flux_list_row_h(FluxListViewData const *ld, int row);
float             flux_list_item_size(FluxListViewData const *ld, FluxListItemData const *it);
float             flux_list_size_for(FluxListViewData const *ld, FluxListItemData const *it, int row);
bool              flux_flow_advance(FluxFlowCursor *at, int line, float h);
int               flux_list_row_at(FluxListViewData const *ld, float y);
float             flux_list_extent(FluxListViewData const *ld);
void              flux_list_flow_seed(FluxListViewData *ld, int first, int last);
bool              flux_list_flow_sync(FluxListViewData *ld);

void              flux_ranges_clear(FluxListViewData *ld);
bool              flux_ranges_contains(FluxListViewData const *ld, int index);
void              flux_ranges_add(FluxListViewData *ld, int first, int last);
void              flux_ranges_remove(FluxListViewData *ld, int index);
void              flux_ranges_cut(FluxListViewData *ld, int lo, int hi);
void              flux_ranges_xor(FluxListViewData *ld, int lo, int hi);

bool              flux_list_grouped(FluxListViewData const *ld);
int               flux_list_group_of_row(FluxListViewData const *ld, int row);
int               flux_list_item_of_row(FluxListViewData const *ld, int row);
int               flux_list_row_of_item(FluxListViewData const *ld, int item);
int               flux_list_detached(FluxListViewData const *ld);
int               flux_list_headers_in(FluxListViewData const *ld, int first, int end);
void              flux_list_relines(FluxListViewData *ld);
int               flux_list_lines(FluxListViewData const *ld);
bool              flux_list_line_header(FluxListViewData const *ld, int line);
int               flux_list_line_of(FluxListViewData const *ld, int row);
int               flux_list_col_of(FluxListViewData const *ld, int row);
int               flux_list_line_first(FluxListViewData const *ld, int line);
int               flux_list_line_last(FluxListViewData const *ld, int line);
bool              flux_list_setgroups(FluxListViewData *ld, void const *sizes, int count);

#endif
