#include "controls/behavior/flux_list_internal.h"

#include <math.h>
#include <stdlib.h>

enum
{
	FLOW_SIZES = 1000,
};

#define FLOW_ITEM_FALLBACK   40.0f
#define FLOW_HEADER_FALLBACK 48.0f

bool flux_list_measured(FluxListViewData const *ld) {
	return ld->ngroup > 0 || (ld->item_height <= 0.0f && ld->item_width <= 0.0f && ld->columns <= 1);
}

static float ring_mean(FluxSizeRing const *r, float fallback) {
	return r->stored > 0 ? ( float ) (r->total / r->stored) : fallback;
}

float flux_list_pitch(FluxListViewData const *ld) {
	float fallback = ld->item_height > 0.0f ? ld->item_height : FLOW_ITEM_FALLBACK;
	return flux_list_measured(ld) ? ring_mean(&ld->item_sizes, fallback) : ld->item_height;
}

static float flow_header_pitch(FluxListViewData const *ld) {
	return ring_mean(&ld->header_sizes, FLOW_HEADER_FALLBACK);
}

static float flow_span(FluxListViewData const *ld, int first, int end) {
	if (end <= first) return 0.0f;
	int headers = flux_list_headers_in(ld, first, end);
	return ( float ) headers * flow_header_pitch(ld) + ( float ) (end - first - headers) * flux_list_pitch(ld);
}

float flux_list_item_size(FluxListViewData const *ld, FluxListItemData const *it) {
	if (it && it->extent >= 0.0f) return it->extent;
	return it && it->header ? flow_header_pitch(ld) : flux_list_pitch(ld);
}

float flux_list_size_for(FluxListViewData const *ld, FluxListItemData const *it, int row) {
	if (it && it->row == row && it->extent >= 0.0f) return it->extent;
	if (!flux_list_measured(ld)) return ld->item_height;
	return flux_list_item_of_row(ld, row) < 0 ? flow_header_pitch(ld) : flux_list_pitch(ld);
}

static int  flow_first(FluxListViewData const *ld) { return flux_list_line_of(ld, ld->realized_first); }

static int  flow_last(FluxListViewData const *ld) { return flux_list_line_of(ld, ld->realized_last); }

static bool flow_in_block(FluxListViewData const *ld, FluxListItemData const *it) {
	return it && it->row >= ld->realized_first && it->row <= ld->realized_last;
}

static bool flow_block(FluxListViewData const *ld) {
	return flux_list_measured(ld) && ld->realized_last >= ld->realized_first;
}

bool flux_flow_advance(FluxFlowCursor *at, int line, float h) {
	bool fresh = line != at->line;
	if (fresh) {
		at->y    += at->h;
		at->line  = line;
		at->h     = 0.0f;
	}
	if (h > at->h) at->h = h;
	return fresh;
}

static FluxListItemData const *flow_find(FluxListViewData const *ld, int line) {
	for (XentNodeId c = flux_list_next_row(ld, XENT_NODE_INVALID); c != XENT_NODE_INVALID;
	  c               = flux_list_next_row(ld, c))
	{
		FluxListItemData const *it = flux_list_item(ld, c);
		if (it && it->line == line) return it;
	}
	return NULL;
}

static int flow_find_y(FluxListViewData const *ld, float y) {
	for (XentNodeId c = flux_list_next_row(ld, XENT_NODE_INVALID); c != XENT_NODE_INVALID;
	  c               = flux_list_next_row(ld, c))
	{
		FluxListItemData const *it = flux_list_item(ld, c);
		if (flow_in_block(ld, it) && y >= it->logical_y && y < it->logical_y + it->extent) return it->line;
	}
	return flow_last(ld);
}

float flux_list_row_y(FluxListViewData const *ld, int line) {
	if (!flow_block(ld)) return ld->header_h + flow_span(ld, 0, line);
	if (line < flow_first(ld)) return ld->block_top - flow_span(ld, line, flow_first(ld));
	if (line > flow_last(ld)) return ld->block_bottom + flow_span(ld, flow_last(ld) + 1, line);
	FluxListItemData const *it = flow_find(ld, line);
	return it ? it->logical_y : ld->block_top + flow_span(ld, flow_first(ld), line);
}

float flux_list_row_h(FluxListViewData const *ld, int line) {
	FluxListItemData const *it = flow_block(ld) ? flow_find(ld, line) : NULL;
	if (it) return flux_list_item_size(ld, it);
	return flux_list_line_header(ld, line) ? flow_header_pitch(ld) : flux_list_pitch(ld);
}

static int flow_search(FluxListViewData const *ld, float y) {
	int lo = 0, hi = flux_list_lines(ld) - 1;
	while (lo < hi) {
		int mid = (lo + hi + 1) / 2;
		if (flux_list_row_y(ld, mid) <= y) lo = mid;
		else hi = mid - 1;
	}
	return lo;
}

static int flow_line(FluxListViewData const *ld, float y) {
	if (!flux_list_measured(ld)) return ( int ) floorf((y - ld->header_h) / ld->item_height);
	if (flow_block(ld) && y >= ld->block_top && y < ld->block_bottom) return flow_find_y(ld, y);
	return flow_search(ld, y);
}

int flux_list_row_at(FluxListViewData const *ld, float y) {
	int line  = flow_line(ld, y);
	int lines = flux_list_lines(ld);
	if (line > lines - 1) line = lines - 1;
	return line > 0 ? line : 0;
}

float flux_list_extent(FluxListViewData const *ld) {
	int lines = flux_list_lines(ld);
	if (!flow_block(ld)) return ld->header_h + flow_span(ld, 0, lines);
	return ld->block_bottom + flow_span(ld, flow_last(ld) + 1, lines);
}

void flux_list_flow_seed(FluxListViewData *ld, int first, int last) {
	if (!flux_list_measured(ld) || last < first) return;
	int lf = flux_list_line_of(ld, first), ll = flux_list_line_of(ld, last);
	if (!ld->pinned || ld->pin < lf || ld->pin > ll) {
		ld->pin_y  = flux_list_row_y(ld, lf);
		ld->pin    = lf;
		ld->pinned = true;
	}
	float top = ld->pin_y;
	for (int line = lf; line < ld->pin; line++) top -= flux_list_row_h(ld, line);
	ld->block_top = top;
}

static void flow_register(FluxListViewData *ld, int line, float size) {
	FluxSizeRing *ring = flux_list_line_header(ld, line) ? &ld->header_sizes : &ld->item_sizes;
	if (!ring->slots) ring->slots = ( float * ) calloc(FLOW_SIZES, sizeof(float));
	if (!ring->slots) return;
	float *slot = &ring->slots [line % FLOW_SIZES];
	if (*slot == size) return;
	ring->total  += size - *slot;
	ring->stored += (*slot == 0.0f) - (size == 0.0f);
	*slot         = size;
}

static void flow_measure(FluxListViewData *ld) {
	FluxFlowCursor at    = {-1, 0.0f, 0.0f};
	float          above = 0.0f;
	for (XentNodeId c = flux_list_next_row(ld, XENT_NODE_INVALID); c != XENT_NODE_INVALID;
	  c               = flux_list_next_row(ld, c))
	{
		FluxListItemData *it = flux_list_item(ld, c);
		XentRect          r  = {0};
		if (!flow_in_block(ld, it) || !xent_layout_rect(ld->ctx, c, &r)) continue;
		it->extent = r.h;
		if (flux_flow_advance(&at, it->line, r.h) && it->line == ld->pin) above = at.y;
		flow_register(ld, it->line, at.h);
	}
	ld->block_top    = ld->pin_y - above;
	ld->block_bottom = ld->block_top + at.y + at.h;
}

static bool flow_placed(FluxListViewData const *ld) {
	FluxFlowCursor at = {-1, ld->block_top, 0.0f};
	for (XentNodeId c = flux_list_next_row(ld, XENT_NODE_INVALID); c != XENT_NODE_INVALID;
	  c               = flux_list_next_row(ld, c))
	{
		FluxListItemData const *it = flux_list_item(ld, c);
		if (!flow_in_block(ld, it)) continue;
		flux_flow_advance(&at, it->line, flux_list_item_size(ld, it));
		if (it->logical_y != at.y) return false;
	}
	return true;
}

static void flow_shift(FluxListViewData *ld, float delta) {
	FluxScrollData *sd  = flux_list_scroll(ld);
	ld->pin_y          += delta;
	ld->block_top      += delta;
	ld->block_bottom   += delta;
	if (!sd) return;
	sd->scroll_y        = fmaxf(0.0f, sd->scroll_y + delta);
	sd->wheel_from_y   += delta;
	sd->wheel_target_y += delta;
}

static float flow_correction(FluxListViewData const *ld) {
	if (flow_first(ld) == 0) return ld->header_h - ld->block_top;
	if (ld->block_top >= ld->header_h) return 0.0f;
	return ld->header_h + flow_span(ld, 0, flow_first(ld)) - ld->block_top;
}

static void flow_repin(FluxListViewData *ld) {
	FluxScrollData *sd     = flux_list_scroll(ld);
	float           scroll = sd ? sd->scroll_y : 0.0f;
	for (XentNodeId c = flux_list_next_row(ld, XENT_NODE_INVALID); c != XENT_NODE_INVALID;
	  c               = flux_list_next_row(ld, c))
	{
		FluxListItemData const *it = flux_list_item(ld, c);
		if (!flow_in_block(ld, it)) continue;
		ld->pin   = it->line;
		ld->pin_y = it->logical_y;
		if (it->logical_y + flux_list_item_size(ld, it) > scroll) return;
	}
}

bool flux_list_flow_sync(FluxListViewData *ld) {
	if (!flow_block(ld) || !ld->pinned) return false;
	flow_measure(ld);
	float delta = flow_correction(ld);
	if (delta != 0.0f) flow_shift(ld, delta);
	bool moved = !flow_placed(ld);
	if (moved) flux_list_layout_block(ld);
	flow_repin(ld);
	return moved;
}
