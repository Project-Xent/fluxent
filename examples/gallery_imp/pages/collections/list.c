#include "app.h"
#include <math.h>
#include <stdio.h>

#define LIST_ROWS 100000

typedef struct ListDemo {
	GalleryApp    *g;
	XentNodeId     list;
	int            count;
	float          item_h;
	float          item_w;
	float          viewport_h;
	int            cols;
	int            selected;
	XtkListKind    kind;
	XtkListSelMode mode;
	void           (*fill)(GalleryApp *g, XentNodeId item, int index);
} ListDemo;

static char const *const kFirst []
  = {"Ada", "Grace", "Alan", "Edsger", "Donald", "Barbara", "John", "Ken", "Dennis", "Bjarne", "Anders", "Lin"};

static char const *const kLast [] = {
  "Lovelace", "Hopper", "Turing", "Dijkstra", "Knuth", "Liskov", "Backus", "Thompson", "Ritchie", "Stroustrup",
  "Hejlsberg", "Xu"};

static void on_list_select(void *ud, int idx) {
	GalleryBind *b = ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = idx});
}

static void list_slot_xy(ListDemo const *ld, int index, int cols, float *x, float *y) {
	*y = ( float ) (index / cols) * ld->item_h;
	*x = ( float ) (index % cols) * ld->item_w;
}

static void list_clear_host(GalleryApp *g, XentNodeId host) {
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

static void list_realize(ListDemo *ld) {
	FluxStore *store = gallery_store(ld->g);
	float      off = 0.0f, ext = ld->viewport_h, cross = 0.0f;
	( void ) flux_list_viewport(store, ld->list, &off, &ext, &cross);
	int cols = xtk_list_auto_columns(cross, ld->item_w, ld->cols);
	int first, last;
	list_visible_range(ld, off, ext, cols, &first, &last);

	XentNodeId host = flux_list_content_node(store, ld->list);
	list_clear_host(ld->g, host);
	for (int idx = first; idx <= last; idx++) {
		XentNodeId item
		  = flux_listitem_init(&(FluxListItemDesc) {.ctx = gallery_ctx(ld->g), .store = store, .parent = host});
		float x, y;
		list_slot_xy(ld, idx, cols, &x, &y);
		fluxlistitem_set_place(store, item, idx, x, y);
		bool sel = flux_list_is_selected(store, ld->list, idx);
		fluxlistitem_set_state(store, item, sel, ld->mode == XTK_LIST_SELECT_MULTIPLE);
		if (ld->fill) ld->fill(ld->g, item, idx);
	}
	flux_list_set_realized(store, ld->list, first, last, cols);
}

static void list_on_stale(void *ctx) {
	list_realize(( ListDemo * ) ctx);
	flux_app_redraw((( ListDemo * ) ctx)->g->flux);
}

static void fill_list_row(GalleryApp *g, XentNodeId item, int index) {
	char const *first = kFirst [index % (sizeof(kFirst) / sizeof(kFirst [0]))];
	char const *last  = kLast [(index / 7) % (sizeof(kLast) / sizeof(kLast [0]))];
	XentNodeId  row   = g_row(g, item, 12.0f);
	XentNodeId  idx   = g_text(g, row, gallery_fmt(g, "%d", index), 12.0f, FLUX_FONT_REGULAR);
	xent_setsize(gallery_ctx(g), idx, (XentSize) {64.0f, NAN});
	g_text(g, row, gallery_fmt(g, "%s %s", first, last), 14.0f, FLUX_FONT_REGULAR);
}

static XentNodeId make_list(
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
	  .on_select = select_bind ? on_list_select : NULL,
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
	  .mode       = mode,
	  .fill       = fill,
	};
	flux_list_onstale(gallery_store(g), list, list_on_stale, demo);
	list_realize(demo);
	if (tag != MSG_NONE) {
		g->live_control [tag] = list;
		g->live_data [tag]    = demo;
	}
	return list;
}

void page_listview(GalleryApp *g, XentNodeId parent) {
	gallery_page_sync(g, page_listview_sync);
	Model const *m      = &g->model;
	char const  *status = m->list_sel >= 0 ? gallery_fmt(g, "Selected row: %d", m->list_sel) : "Nothing selected yet.";
	g_page_header(
	  g, parent, "ListView",
	  "A virtualized list: 100,000 rows, but only the viewport's worth of controls exist. "
	  "Scrolling recycles rows in place."
	);
	{
		XentNodeId ex  = g_example(g, parent, "Click a row to select it.");
		XentNodeId col = g_col(g, ex, 12.0f, 0, 0, 0, 0);
		make_list(
		  g, col, XTK_LIST_KIND_LIST, LIST_ROWS, 40.0f, 0.0f, 1, m->list_sel, XTK_LIST_SELECT_SINGLE, MSG_LIST_SELECT,
		  fill_list_row, 420.0f
		);
		g->live_output [MSG_LIST_SELECT] = g_text(g, col, status, 13.0f, FLUX_FONT_REGULAR);
	}
	{
		XentNodeId ex
		  = g_example(g, parent, "SelectionMode Extended: Ctrl+click toggles, Shift+click ranges, Ctrl+A selects all.");
		XentNodeId col = g_col(g, ex, 12.0f, 0, 0, 0, 0);
		make_list(
		  g, col, XTK_LIST_KIND_LIST, 200, 40.0f, 0.0f, 1, -1, XTK_LIST_SELECT_EXTENDED, MSG_MULTI_SELECT,
		  fill_list_row, 240.0f
		);
		g->live_output [MSG_MULTI_SELECT] = g_text(
		  g, col,
		  m->multi_lead >= 0 ? gallery_fmt(g, "Lead selection: row %d", m->multi_lead)
							 : "Selection is control-retained; the lead index posts here.",
		  13.0f, FLUX_FONT_REGULAR
		);
	}
}

static void sync_list(GalleryApp *g, int tag) {
	ListDemo *demo = g->live_data [tag];
	if (demo) list_realize(demo);
}

void page_listview_sync(GalleryApp *g) {
	flux_list_set_selected(gallery_store(g), g->live_control [MSG_LIST_SELECT], g->model.list_sel);
	sync_list(g, MSG_LIST_SELECT);
	sync_list(g, MSG_MULTI_SELECT);
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_LIST_SELECT],
	  g->model.list_sel >= 0 ? gallery_fmt(g, "Selected row: %d", g->model.list_sel) : "Nothing selected yet."
	);
	xent_settext(
	  gallery_ctx(g), g->live_output [MSG_MULTI_SELECT],
	  g->model.multi_lead >= 0 ? gallery_fmt(g, "Lead selection: row %d", g->model.multi_lead)
							   : "Selection is control-retained; the lead index posts here."
	);
}
