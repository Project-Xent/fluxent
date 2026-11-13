#include "app.h"

static void grid_cell(GalleryApp *g, XtkTxn *txn, XtkNodeRef item, int index) {
	XtkLocalId col = ui_column(txn, item, 0.0f, XENT_FLEX_ALIGN_CENTER);
	ui_center(txn, col);
	ui_text(txn, xtk_ref_local(col), gallery_fmt(g, "%d", index), 20.0f, FLUX_FONT_SEMI_BOLD);
	ui_text(txn, xtk_ref_local(col), gallery_fmt(g, "Item %d", index), 11.0f, FLUX_FONT_REGULAR);
}

static char const *grid_text(GalleryApp *g) {
	return g->model.grid_sel >= 0 ? gallery_fmt(g, "Selected cell: %d", g->model.grid_sel) : "Nothing selected yet.";
}

static void gridview_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_GRID_SELECT], grid_text(g)); }

bool        page_gridview_build(PageBuild *b) {
	page_sync(b, gridview_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "GridView",
	  "The same virtualized items host wrapping cells across auto-fit columns. Arrow keys navigate in 2D; the "
	  "selection "
	  "ring is the WinUI accent border."
	);
	XtkNodeRef col  = xtk_ref_local(ui_column(
	  &b->txn, page_example(b, parent, "10,000 uniform cells; columns follow the width."), 12.0f,
	  XENT_FLEX_ALIGN_STRETCH
	));
	XtkLocalId grid = xtk_gridview_create(
	  &b->txn, col,
	  &(XtkListProps) {
		.set   = XTK_LIST_H_MASK | XTK_LIST_SEL_MASK | XTK_LIST_NITEM_MASK | XTK_LIST_ITEMH_MASK | XTK_LIST_ITEMW_MASK,
		.h     = 420.0f,
		.sel   = b->app->model.grid_sel,
		.nitem = 10000,
		.itemh = 104.0f,
		.itemw = 152.0f,
	  }
	);
	xtk_gridview_onselect(&b->txn, xtk_ref_local(grid), page_post, page_bind(b, MSG_GRID_SELECT));
	page_list(b, grid, grid_cell);
	page_keepout(b, MSG_GRID_SELECT, page_note(b, col, grid_text(b->app)));
	return true;
}
