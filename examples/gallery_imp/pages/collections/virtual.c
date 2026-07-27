#include "virtual.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host/flux_prop_apply.h"

void gallery_list_select(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

void gallery_list_invoke(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

static void list_slot_xy(ListDemo const *ld, int index, int cols, float *x, float *y) {
	*y = ( float ) (index / cols) * ld->item_h;
	*x = ( float ) (index % cols) * ld->item_w;
}

void gallery_list_clear(GalleryApp *g, XentNodeId host) {
	XentCtx *ctx = gallery_ctx(g);
	for (XentNodeId c = xent_node_first(ctx, host); c != XENT_NODE_INVALID;) {
		XentNodeId next = xent_node_next(ctx, c);
		flux_tree_drop(gallery_store(g), c);
		c = next;
	}
}

static void list_visible_range(ListDemo const *ld, float offset, float extent, int cols, int *first, int *last) {
	int first_row = ( int ) (offset / ld->item_h);
	if (first_row > 4) first_row -= 4;
	else first_row = 0;
	int last_row = ( int ) ((offset + extent) / ld->item_h) + 4;
	*first       = first_row * cols;
	*last        = (last_row + 1) * cols - 1;
	if (*last >= ld->count) *last = ld->count - 1;
	if (*last < *first) *last = *first;
}

void gallery_list_realize(ListDemo *ld) {
	FluxStore *store = gallery_store(ld->g);
	float      off = 0.0f, ext = ld->viewport_h, cross = 0.0f;
	( void ) flux_list_viewport(store, ld->list, &off, &ext, &cross);
	int cols = xtk_list_auto_columns(cross, ld->item_w, ld->cols);
	int first, last;
	list_visible_range(ld, off, ext, cols, &first, &last);

	XentNodeId host = flux_list_content_node(store, ld->list);
	gallery_list_clear(ld->g, host);
	for (int idx = first; idx <= last; idx++) {
		XentNodeId item
		  = flux_listitem_init(&(FluxListItemDesc) {.ctx = gallery_ctx(ld->g), .store = store, .parent = host});
		float x, y;
		list_slot_xy(ld, idx, cols, &x, &y);
		fluxlistitem_set_place(store, item, idx, x, y);
		bool sel = flux_list_is_selected(store, ld->list, idx);
		fluxlistitem_set_state(store, item, sel, false);
		if (ld->fill) ld->fill(ld->g, item, idx);
	}
	flux_list_set_realized(store, ld->list, first, last, cols);
}

static void list_on_stale(void *ctx) {
	gallery_list_realize(( ListDemo * ) ctx);
	flux_app_redraw((( ListDemo * ) ctx)->g->flux);
}

XentNodeId gallery_list_create(
  GalleryApp *g, XentNodeId parent, XtkListKind kind, int count, float item_h, float item_w, int cols, int selected,
  XtkListSelMode mode, int tag, void (*fill)(GalleryApp *, XentNodeId, int), float height
) {
	GalleryBind *select_bind = tag != MSG_NONE ? gallery_bind(g, tag) : NULL;
	XentNodeId   list        = flux_list_init(&(FluxListViewDesc) {
	  .ctx       = gallery_ctx(g),
	  .store     = gallery_store(g),
	  .parent    = parent,
	  .kind      = kind,
	  .input     = flux_app_input(g->flux),
	  .on_select = select_bind ? gallery_list_select : NULL,
	  .userdata  = select_bind,
	});
	xent_seth(gallery_ctx(g), list, height);
	flux_list_set_extent(gallery_store(g), list, count, item_h, item_w, cols);
	flux_list_setmode(gallery_store(g), list, mode);
	if (mode == XTK_LIST_SELECT_SINGLE) flux_list_set_selected(gallery_store(g), list, selected);
	ListDemo *demo = ( ListDemo * ) gallery_page_alloc(g, sizeof(*demo));
	if (!demo) return XENT_NODE_INVALID;
	*demo = (ListDemo) {
	  .g          = g,
	  .list       = list,
	  .count      = count,
	  .item_h     = item_h,
	  .item_w     = item_w,
	  .viewport_h = height,
	  .cols       = cols,
	  .selected   = selected,
	  .kind       = kind,
	  .fill       = fill,
	};
	flux_list_onstale(gallery_store(g), list, list_on_stale, demo);
	gallery_list_realize(demo);
	if (tag != MSG_NONE) {
		g->live_control [tag] = list;
		g->live_data [tag]    = demo;
	}
	return list;
}
