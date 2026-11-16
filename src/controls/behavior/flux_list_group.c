#include "controls/behavior/flux_list_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int group_search(int const *starts, int n, int value) {
	int lo = 0, hi = n - 1;
	while (lo < hi) {
		int mid = (lo + hi + 1) / 2;
		if (starts [mid] <= value) lo = mid;
		else hi = mid - 1;
	}
	return lo;
}

bool       flux_list_grouped(FluxListViewData const *ld) { return ld->ngroup > 0; }

static int group_lower(int const *starts, int n, int value) {
	int lo = 0, hi = n;
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		if (starts [mid] < value) lo = mid + 1;
		else hi = mid;
	}
	return lo;
}

int flux_list_headers_in(FluxListViewData const *ld, int first, int end) {
	if (!flux_list_grouped(ld) || end <= first) return 0;
	return group_lower(ld->group_line, ld->ngroup, end) - group_lower(ld->group_line, ld->ngroup, first);
}

static int list_cols(FluxListViewData const *ld) { return ld->cols > 0 ? ld->cols : 1; }

static int group_lines(FluxListViewData const *ld, int g) {
	return 1 + (ld->groups [g] + list_cols(ld) - 1) / list_cols(ld);
}

void flux_list_relines(FluxListViewData *ld) {
	int line = 0;
	for (int g = 0; g < ld->ngroup; g++) {
		ld->group_line [g]  = line;
		line               += group_lines(ld, g);
	}
}

int flux_list_lines(FluxListViewData const *ld) {
	if (!flux_list_grouped(ld)) return (ld->count + list_cols(ld) - 1) / list_cols(ld);
	return ld->group_line [ld->ngroup - 1] + group_lines(ld, ld->ngroup - 1);
}

bool flux_list_line_header(FluxListViewData const *ld, int line) {
	return flux_list_grouped(ld) && ld->group_line [group_search(ld->group_line, ld->ngroup, line)] == line;
}

int flux_list_line_of(FluxListViewData const *ld, int row) {
	if (!flux_list_grouped(ld)) return row / list_cols(ld);
	int g = group_search(ld->group_row, ld->ngroup, row);
	return row == ld->group_row [g] ? ld->group_line [g]
	                                : ld->group_line [g] + 1 + (row - ld->group_row [g] - 1) / list_cols(ld);
}

int flux_list_col_of(FluxListViewData const *ld, int row) {
	if (!flux_list_grouped(ld)) return row % list_cols(ld);
	int g = group_search(ld->group_row, ld->ngroup, row);
	return row == ld->group_row [g] ? 0 : (row - ld->group_row [g] - 1) % list_cols(ld);
}

int flux_list_line_first(FluxListViewData const *ld, int line) {
	if (!flux_list_grouped(ld)) return line * list_cols(ld);
	int g = group_search(ld->group_line, ld->ngroup, line);
	if (line == ld->group_line [g]) return ld->group_row [g];
	return ld->group_row [g] + 1 + (line - ld->group_line [g] - 1) * list_cols(ld);
}

int flux_list_line_last(FluxListViewData const *ld, int line) {
	int first = flux_list_line_first(ld, line);
	int last  = first + list_cols(ld) - 1;
	if (!flux_list_grouped(ld)) return last < ld->count - 1 ? last : ld->count - 1;
	int g = group_search(ld->group_line, ld->ngroup, line);
	if (line == ld->group_line [g]) return first;
	return last < ld->group_row [g] + ld->groups [g] ? last : ld->group_row [g] + ld->groups [g];
}

int flux_list_group_of_row(FluxListViewData const *ld, int row) {
	return flux_list_grouped(ld) ? group_search(ld->group_row, ld->ngroup, row) : -1;
}

int flux_list_item_of_row(FluxListViewData const *ld, int row) {
	if (!flux_list_grouped(ld)) return row;
	int g = group_search(ld->group_row, ld->ngroup, row);
	return row == ld->group_row [g] ? -1 : ld->group_item [g] + row - ld->group_row [g] - 1;
}

int flux_list_row_of_item(FluxListViewData const *ld, int item) {
	if (!flux_list_grouped(ld)) return item;
	int g = group_search(ld->group_item, ld->ngroup, item);
	while (g > 0 && ld->groups [g] == 0 && ld->group_item [g] == item) g--;
	while (g < ld->ngroup - 1 && item >= ld->group_item [g] + ld->groups [g]) g++;
	return ld->group_row [g] + 1 + item - ld->group_item [g];
}

int flux_list_detached(FluxListViewData const *ld) {
	if (!flux_list_grouped(ld) || ld->realized_last < ld->realized_first) return -1;
	if (flux_list_item_of_row(ld, ld->realized_first) < 0) return -1;
	return ld->group_row [flux_list_group_of_row(ld, ld->realized_first)];
}

bool flux_list_setgroups(FluxListViewData *ld, void const *sizes, int count) {
	free(ld->groups);
	ld->groups     = NULL;
	ld->group_row  = NULL;
	ld->group_item = NULL;
	ld->group_line = NULL;
	ld->ngroup     = 0;
	if (!sizes || count <= 0) return true;
	int *block = ( int * ) malloc(sizeof(int) * 4u * ( size_t ) count);
	if (!block) return false;
	ld->groups     = block;
	ld->group_row  = block + count;
	ld->group_item = block + 2 * count;
	ld->group_line = block + 3 * count;
	ld->ngroup     = count;
	int row = 0, item = 0;
	for (int g = 0; g < count; g++) {
		int32_t size;
		memcpy(&size, ( char const * ) sizes + sizeof(size) * ( size_t ) g, sizeof(size));
		ld->groups [g]      = size > 0 ? size : 0;
		ld->group_row [g]   = row;
		ld->group_item [g]  = item;
		row                += 1 + ld->groups [g];
		item               += ld->groups [g];
	}
	ld->count = item;
	flux_list_relines(ld);
	return true;
}

static float group_bottom(FluxListViewData const *ld, int g) {
	if (g + 1 >= ld->ngroup) return flux_list_extent(ld);
	return flux_list_row_y(ld, ld->group_line [g + 1]);
}

static float header_height(FluxListViewData const *ld, XentNodeId node) {
	XentRect r = {0};
	return xent_layout_rect(ld->ctx, node, &r) ? r.h : 0.0f;
}

static XentNodeId stuck_header(FluxListViewData const *ld, float top) {
	XentNodeId best   = XENT_NODE_INVALID;
	float      best_y = -INFINITY;
	for (XentNodeId c = flux_list_next_row(ld, XENT_NODE_INVALID); c != XENT_NODE_INVALID;
	  c               = flux_list_next_row(ld, c))
	{
		FluxListItemData const *it = flux_list_item(ld, c);
		if (!it || !it->header || it->logical_y > top || it->logical_y < best_y) continue;
		best   = c;
		best_y = it->logical_y;
	}
	return best;
}

static void stick_apply(FluxListViewData const *ld, XentNodeId stuck, float translate, float line) {
	for (XentNodeId c = flux_list_next_row(ld, XENT_NODE_INVALID); c != XENT_NODE_INVALID;
	  c               = flux_list_next_row(ld, c))
	{
		FluxListItemData const *it = flux_list_item(ld, c);
		FluxNodeData           *nd = flux_store_find(ld->store, c);
		if (!it || !nd) continue;
		nd->render_translate_y = c == stuck ? translate : 0.0f;
		nd->clip_top           = it->header ? 0.0f : fmaxf(0.0f, line - it->logical_y);
	}
}

uint32_t flux_list_stick(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) {
	( void ) ctx;
	( void ) store;
	( void ) node;
	FluxListViewData *ld = ( FluxListViewData * ) nd->component_data;
	FluxScrollData   *sd = ld && flux_list_grouped(ld) ? flux_list_scroll(ld) : NULL;
	if (!sd) return 0u;
	float      top   = sd->scroll_y;
	XentNodeId stuck = stuck_header(ld, top);
	if (stuck == XENT_NODE_INVALID) {
		stick_apply(ld, stuck, 0.0f, -INFINITY);
		return 0u;
	}
	FluxListItemData const *it = flux_list_item(ld, stuck);
	float                   h  = header_height(ld, stuck);
	float                   y  = fminf(top, group_bottom(ld, it->group) - h);
	if (y < it->logical_y) y = it->logical_y;
	stick_apply(ld, stuck, y - it->logical_y, y + h);
	return 0u;
}
