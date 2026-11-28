#include "app.h"

#include <math.h>

#define DET_CELL_PAD 8.0f
#define DET_SLOT_ROW DET_COLS

static int              det_built [DET_COLS];

static XtkSortDirection det_sort(Model const *m, int col) {
	return m->det_sort != col ? XTK_SORT_NONE : m->det_desc ? XTK_SORT_DESCENDING : XTK_SORT_ASCENDING;
}

static uint32_t det_reorder(void *arg, XtkEvent const *event) {
	GalleryBind const *bind = ( GalleryBind const * ) arg;
	gallery_post(bind->app, (XtkMsg) {.tag = MSG_DET_REORDER, .d = bind->i, .i = event->payload.u.i32});
	return XTK_EVENT_HANDLED;
}

static void det_column(PageBuild *b, XtkNodeRef header, int slot, int col) {
	Model const *m     = &b->app->model;
	XtkLocalId   local = xtk_colitem_create(
	  b->txn, header,
	  &(XtkColumnProps) {
		.set  = XTK_COLUMN_W_MASK | XTK_COLUMN_TEXT_MASK | XTK_COLUMN_SORT_MASK,
		.w    = m->det_width [col],
		.text = kDetHeaders [col],
		.sort = det_sort(m, col),
	  }
	);
	XtkNodeRef ref = xtk_ref_local(local);
	xtk_colitem_oninvoke(b->txn, ref, page_post, page_bind_i(b, MSG_DET_SORT, col));
	xtk_colitem_onresize(b->txn, ref, page_post, page_bind_i(b, MSG_DET_RESIZE, col));
	xtk_colitem_onfit(b->txn, ref, page_post, page_bind_i(b, MSG_DET_FIT, col));
	xtk_colitem_onreorder(b->txn, ref, det_reorder, page_bind_i(b, MSG_DET_REORDER, col));
	page_keepx(b, slot, local);
}

static void det_row(PageBuild *b, XtkNodeRef parent, int r, int const rows [DET_ROWS]) {
	Model const *m   = &b->app->model;
	XtkNodeRef   row = xtk_ref_local(ui_row(b->txn, parent, 0.0f, false));
	for (int p = 0; p < DET_COLS; p++) {
		int        col  = m->det_order [p];
		XtkLocalId cell = ui_text(b->txn, row, gallery_det_cell(rows [r], col), col ? 12.0f : 14.0f, FLUX_FONT_REGULAR);
		ui_size(b->txn, cell, m->det_width [col] - DET_CELL_PAD, NAN);
		xtk_txn_set(
		  b->txn, xtk_ref_local(cell), XTK_PROP_MARGIN_LEFT, (XtkValue) {.kind = XTK_VALUE_F32, .u.f32 = DET_CELL_PAD},
		  (XtkChangeStamp) {0}
		);
		page_keepx(b, DET_SLOT_ROW + r * DET_COLS + p, cell);
	}
}

static void det_sync_cells(GalleryApp *g, int const rows [DET_ROWS]) {
	for (int r = 0; r < DET_ROWS; r++)
		for (int p = 0; p < DET_COLS; p++) {
			XtkCtrl cell = g->live_extra [DET_SLOT_ROW + r * DET_COLS + p];
			int     col  = g->model.det_order [p];
			gallery_settext(g, cell, gallery_det_cell(rows [r], col));
			xtk_ctrl_setf32(
			  &g->scene, cell, XTK_PROP_WIDTH, g->model.det_width [col] - DET_CELL_PAD, (XtkChangeStamp) {0}
			);
		}
}

static void det_sync(GalleryApp *g) {
	Model const *m = &g->model;
	for (int p = 0; p < DET_COLS; p++)
		if (det_built [p] != m->det_order [p]) g->rebuild_request = true;
	if (g->rebuild_request) return;
	for (int p = 0; p < DET_COLS; p++) {
		int col = m->det_order [p];
		xtk_colitem_setw(&g->scene, g->live_extra [p], m->det_width [col], (XtkChangeStamp) {0});
		xtk_colitem_setsort(&g->scene, g->live_extra [p], det_sort(m, col), (XtkChangeStamp) {0});
	}
	int rows [DET_ROWS];
	gallery_det_rows(m, rows);
	det_sync_cells(g, rows);
}

bool page_details_build(PageBuild *b) {
	page_sync(b, det_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(b, parent, "Details header", "File Explorer's column header over a list, as Files builds it.");
	XtkNodeRef card = page_example(
	  b, parent, "Click a header to sort, drag a divider to resize, double-click it to fit, drag a header to reorder."
	);
	XtkNodeRef list   = xtk_ref_local(xtk_list_create(
	  b->txn, card,
	  &(XtkListProps) {
		.set   = XTK_LIST_H_MASK | XTK_LIST_SEL_MASK | XTK_LIST_NITEM_MASK | XTK_LIST_ITEMH_MASK,
		.h     = 420.0f,
		.sel   = -1,
		.nitem = DET_ROWS,
		.itemh = 36.0f,
	  }
	));
	XtkNodeRef top    = xtk_ref_local(xtk_container_create(
	  b->txn, list,
	  &(XtkContainerProps) {
		.set  = XTK_CONTAINER_DIR_MASK
	          | XTK_CONTAINER_PL_MASK
	          | XTK_CONTAINER_PR_MASK
	          | XTK_CONTAINER_FITW_MASK
	          | XTK_CONTAINER_FITH_MASK,
		.dir  = XTK_STACK_ROW,
		.pl   = 16.0f,
		.pr   = 12.0f,
		.fitw = true,
		.fith = true,
	  }
	));
	XtkNodeRef header = xtk_ref_local(xtk_colheader_create(b->txn, top, &(XtkLayoutProps) {0}));
	for (int p = 0; p < DET_COLS; p++) {
		det_built [p] = m->det_order [p];
		det_column(b, header, p, m->det_order [p]);
	}
	int rows [DET_ROWS];
	gallery_det_rows(m, rows);
	for (int r = 0; r < DET_ROWS; r++)
		det_row(b, xtk_ref_local(xtk_txn_create(b->txn, XTK_CTRL_LISTITEM, list, xtk_ref_none(), NULL, 0)), r, rows);
	return true;
}
