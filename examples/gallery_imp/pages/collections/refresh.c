#include "app.h"

static void refresh_row(GalleryApp *g, XtkTxn *txn, XtkNodeRef item, int index) {
	ui_text(txn, item, gallery_fmt(g, "Row %d", index), 14.0f, FLUX_FONT_REGULAR);
}

static char const *refresh_text(GalleryApp *g) {
	return gallery_fmt(
	  g, "Refresh count: %d. %s", g->model.refresh_count, g->model.refresh_busy ? "Refreshing…" : "Idle"
	);
}

static void refresh_sync(GalleryApp *g) {
	xtk_refresh_setrefreshing(&g->scene, g->live_control [MSG_REFRESH], g->model.refresh_busy, (XtkChangeStamp) {0});
	xtk_button_setenabled(&g->scene, g->live_control [MSG_REFRESH_DONE], g->model.refresh_busy, (XtkChangeStamp) {0});
	gallery_settext(g, g->live_output [MSG_REFRESH], refresh_text(g));
}

bool page_pull_refresh_build(PageBuild *b) {
	page_sync(b, refresh_sync);
	bool       busy   = b->app->model.refresh_busy;
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "PullToRefresh", "A RefreshContainer wrapping scrollable content. Pull from the top edge to refresh."
	);
	XtkNodeRef col     = xtk_ref_local(ui_column(
	  &b->txn, page_example(b, parent, "List inside a top-edge pull container."), 12.0f, XENT_FLEX_ALIGN_STRETCH
	));
	XtkLocalId refresh = xtk_refresh_create(
	  &b->txn, col,
	  &(XtkRefreshProps) {.set = XTK_REFRESH_H_MASK | XTK_REFRESH_REFRESHING_MASK, .h = 360.0f, .refreshing = busy}
	);
	xtk_refresh_onrefresh(&b->txn, xtk_ref_local(refresh), page_post, page_bind(b, MSG_REFRESH));
	XtkLocalId list = xtk_list_create(
	  &b->txn, xtk_ref_local(refresh),
	  &(XtkListProps) {
		.set   = XTK_LIST_GROW_MASK | XTK_LIST_NITEM_MASK | XTK_LIST_ITEMH_MASK | XTK_LIST_MODE_MASK,
		.grow  = 1.0f,
		.nitem = 40,
		.itemh = 40.0f,
		.mode  = XTK_LIST_SELECT_NONE,
	  }
	);
	page_list(b, list, refresh_row);
	page_keep(b, MSG_REFRESH, refresh);
	page_keepout(b, MSG_REFRESH, page_note(b, col, refresh_text(b->app)));
	XtkLocalId done = page_button(b, col, "End refresh", MSG_REFRESH_DONE, 0);
	ui_enable(&b->txn, done, busy);
	page_keep(b, MSG_REFRESH_DONE, done);
	return true;
}
