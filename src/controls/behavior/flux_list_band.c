#include "controls/behavior/flux_list_internal.h"
#include "fluxent/flux_input.h"
#include "host/flux_prop_apply.h"
#include "render/flux_render_snapshot.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum
{
	BAND_NONE,
	BAND_STARTING,
	BAND_ACTIVE,
};

enum
{
	BAND_IGNORE,
	BAND_EXTEND,
	BAND_INVERT,
};

#define BAND_DELTA 5.0f
#define BAND_EDGE  20.0f
#define BAND_STEP  40.0f

static FluxListSelRange *ranges_dup(FluxListViewData const *ld) {
	FluxListSelRange *copy = ( FluxListSelRange * ) malloc(sizeof(*copy) * ( size_t ) (ld->range_count + 1));
	if (copy && ld->range_count) memcpy(copy, ld->ranges, sizeof(*copy) * ( size_t ) ld->range_count);
	return copy;
}

static void band_end(FluxListViewData *ld) {
	ld->band = BAND_NONE;
	free(ld->band_prev);
	ld->band_prev       = NULL;
	ld->band_prev_count = 0;
}

static void band_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxListViewData *ld    = ( FluxListViewData * ) ctx;
	FluxNodeData     *snd   = flux_store_find(ld->store, ld->scroll);
	FluxScrollData   *sd    = flux_list_scroll(ld);
	bool              multi = ld->sel_mode == XTK_LIST_SELECT_MULTIPLE || ld->sel_mode == XTK_LIST_SELECT_EXTENDED;
	if (!sd || !snd || !multi || snd->state.pointer_type == FLUX_POINTER_TOUCH) return;
	band_end(ld);
	ld->band_prev = ranges_dup(ld);
	if (!ld->band_prev) return;
	ld->band_prev_count = ld->range_count;
	ld->band_mode       = flux_key_held(FLUX_KEY_CONTROL) ? BAND_INVERT
	                    : flux_key_held(FLUX_KEY_SHIFT)   ? BAND_EXTEND
	                                                      : BAND_IGNORE;
	ld->band            = BAND_STARTING;
	ld->band_x = ld->band_cx = x;
	ld->band_y               = y + sd->scroll_y;
	ld->band_cy              = y;
	if (ld->band_mode != BAND_IGNORE || ld->range_count == 0) return;
	flux_ranges_clear(ld);
	flux_list_report(ld, -1);
}

static void band_scroll(FluxListViewData *ld, FluxScrollData const *sd, float y) {
	XentRect r = {0};
	if (!xent_layout_rect(ld->ctx, ld->scroll, &r)) return;
	float step = 0.0f;
	if (y > r.h - BAND_EDGE) step = fminf(y - (r.h - BAND_EDGE), BAND_STEP);
	else if (y < BAND_EDGE) step = -fminf(BAND_EDGE - y, BAND_STEP);
	if (step == 0.0f) return;
	float max = fmaxf(0.0f, sd->content_h - r.h);
	flux_prop_scroll_offset(ld->store, ld->scroll, sd->scroll_x, fminf(max, fmaxf(0.0f, sd->scroll_y + step)));
}

static void band_apply(FluxListViewData *ld, int lo, int hi) {
	if (lo > hi) return;
	if (ld->band_mode == BAND_INVERT) flux_ranges_xor(ld, lo, hi);
	else flux_ranges_add(ld, lo, hi);
}

static int band_item(FluxListViewData const *ld, int row, int end, int step) {
	for (; step > 0 ? row <= end : row >= end; row += step) {
		int item = flux_list_item_of_row(ld, row);
		if (item >= 0) return item;
	}
	return -1;
}

static int band_column(float x, float w, int cols) {
	int c = ( int ) floorf(x / w);
	return c < 0 ? 0 : (c > cols - 1 ? cols - 1 : c);
}

static int band_grid(FluxListViewData *ld, int l0, int l1, float left, float right) {
	int   cols = ld->cols;
	float w    = ld->item_width > 0.0f ? ld->item_width : 1.0f;
	int   c0 = band_column(left, w, cols), c1 = band_column(right, w, cols), hi = -1;
	for (int l = l0; l <= l1; l++) {
		int first = flux_list_line_first(ld, l), last = flux_list_line_last(ld, l);
		int lo_e = first + c0, hi_e = first + c1 < last ? first + c1 : last;
		if (flux_list_line_header(ld, l) || lo_e > hi_e) continue;
		hi = flux_list_item_of_row(ld, hi_e);
		band_apply(ld, flux_list_item_of_row(ld, lo_e), hi);
	}
	return hi;
}

static int band_lines(FluxListViewData *ld, int l0, int l1, float left, float right) {
	if (ld->cols > 1) return band_grid(ld, l0, l1, left, right);
	int lo = band_item(ld, l0, l1, 1), hi = band_item(ld, l1, l0, -1);
	if (lo >= 0 && hi >= lo) band_apply(ld, lo, hi);
	return hi;
}

static void band_select(FluxListViewData *ld, FluxScrollData const *sd) {
	FluxListSelRange *before = ranges_dup(ld);
	int               count  = ld->range_count;
	if (!before) return;
	ld->range_count = 0;
	if (ld->band_mode != BAND_IGNORE)
		for (int i = 0; i < ld->band_prev_count; i++)
			flux_ranges_add(ld, ld->band_prev [i].first, ld->band_prev [i].last);
	float top = fminf(ld->band_y, ld->band_cy + sd->scroll_y), bottom = fmaxf(ld->band_y, ld->band_cy + sd->scroll_y);
	int   lead = -1;
	if (top < flux_list_extent(ld) && bottom > flux_list_row_y(ld, 0))
		lead = band_lines(
		  ld, flux_list_row_at(ld, top), flux_list_row_at(ld, bottom), fminf(ld->band_x, ld->band_cx),
		  fmaxf(ld->band_x, ld->band_cx)
		);
	bool changed = count != ld->range_count || memcmp(before, ld->ranges, sizeof(*before) * ( size_t ) count) != 0;
	free(before);
	if (changed) flux_list_report(ld, lead);
}

static void band_move(void *ctx, float x, float y) {
	FluxListViewData *ld = ( FluxListViewData * ) ctx;
	FluxScrollData   *sd = flux_list_scroll(ld);
	if (ld->band == BAND_NONE || !sd) return;
	ld->band_cx = x;
	ld->band_cy = y;
	if (ld->band == BAND_STARTING) {
		if (fabsf(x - ld->band_x) <= BAND_DELTA && fabsf(y + sd->scroll_y - ld->band_y) <= BAND_DELTA) return;
		ld->band = BAND_ACTIVE;
	}
	band_scroll(ld, sd, y);
	band_select(ld, sd);
}

static bool band_release(void *ctx) {
	band_end(( FluxListViewData * ) ctx);
	return true;
}

static void band_cancel(void *ctx) { band_end(( FluxListViewData * ) ctx); }

void        flux_list_band_enable(FluxListViewData *ld, bool on) {
	FluxNodeData *snd = flux_store_find(ld->store, ld->scroll);
	ld->rect_select   = on;
	if (!snd) return;
	FluxNodeBehavior *bh = &snd->behavior;
	if (!on) {
		band_end(ld);
		bh->on_pointer_down = NULL;
		bh->on_pointer_move = NULL;
		bh->on_release      = NULL;
		bh->on_cancel       = NULL;
		return;
	}
	bh->on_pointer_down     = band_down;
	bh->on_pointer_down_ctx = ld;
	bh->on_pointer_move     = band_move;
	bh->on_pointer_move_ctx = ld;
	bh->on_release          = band_release;
	bh->on_release_ctx      = ld;
	bh->on_cancel           = band_cancel;
	bh->on_cancel_ctx       = ld;
}

void flux_snap_list(FluxSnapCtx const *ctx) {
	FluxListViewData const *ld = ( FluxListViewData const * ) ctx->data;
	FluxScrollData const   *sd = flux_list_scroll(ld);
	XentRect                sr = {0}, rr = {0};
	if (!sd || ld->band != BAND_ACTIVE || !xent_layout_rect(ld->ctx, ld->scroll, &sr)) return;
	xent_layout_rect(ld->ctx, ld->root, &rr);
	float oy = ld->band_y - sd->scroll_y;
	float x0 = fmaxf(0.0f, fminf(ld->band_x, ld->band_cx)), x1 = fminf(sr.w, fmaxf(ld->band_x, ld->band_cx));
	float y0 = fmaxf(0.0f, fminf(oy, ld->band_cy)), y1 = fminf(sr.h, fmaxf(oy, ld->band_cy));
	if (x1 <= x0 || y1 <= y0) return;
	ctx->snap->u.list = (FluxListSnapshot) {
	  true, {sr.x - rr.x + x0, sr.y - rr.y + y0, x1 - x0, y1 - y0}
    };
}
