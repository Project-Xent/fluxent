#include "app.h"

static void items_cell(GalleryApp *g, XtkTxn *txn, XtkNodeRef item, int index) {
	XtkNodeRef card = xtk_ref_local(ui_card(txn, item, 0.0f, XENT_FLEX_ALIGN_CENTER, 12.0f));
	ui_text(txn, card, gallery_fmt(g, "Item %d", index), 14.0f, FLUX_FONT_SEMI_BOLD);
	ui_text(txn, card, "Tap to invoke", 11.0f, FLUX_FONT_REGULAR);
}

static char const *invoke_text(GalleryApp *g) {
	return g->model.items_invoke >= 0 ? gallery_fmt(g, "Invoked: %d", g->model.items_invoke) : "Tap a cell to invoke.";
}

static void items_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_ITEMS_INVOKE], invoke_text(g)); }

bool        page_items_view_build(PageBuild *b) {
	page_sync(b, items_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "ItemsView", "ItemsRepeater + layout + ItemContainer chrome. Here: a wrapping flow of 24 cards."
	);
	XtkNodeRef col  = xtk_ref_local(ui_column(
	  &b->txn, page_example(b, parent, "Flow layout with ItemInvoked (no selection chrome)."), 12.0f,
	  XENT_FLEX_ALIGN_STRETCH
	));
	XtkLocalId view = xtk_itemsview_create(
	  &b->txn, col,
	  &(XtkItemsViewProps) {
		.set     = XTK_ITEMSVIEW_H_MASK
	             | XTK_ITEMSVIEW_NITEM_MASK
	             | XTK_ITEMSVIEW_ITEMH_MASK
	             | XTK_ITEMSVIEW_ITEMW_MASK
	             | XTK_ITEMSVIEW_MODE_MASK
	             | XTK_ITEMSVIEW_INVOKED_MASK,
		.h       = 360.0f,
		.nitem   = 24,
		.itemh   = 96.0f,
		.itemw   = 140.0f,
		.mode    = XTK_LIST_SELECT_NONE,
		.invoked = true,
	  }
	);
	xtk_itemsview_oninvoke(&b->txn, xtk_ref_local(view), page_post, page_bind(b, MSG_ITEMS_INVOKE));
	page_list(b, view, items_cell);
	page_keepout(b, MSG_ITEMS_INVOKE, page_note(b, col, invoke_text(b->app)));
	return true;
}
