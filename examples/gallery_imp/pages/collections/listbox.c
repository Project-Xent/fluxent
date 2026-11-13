#include "app.h"

static char const *const kFonts [] = {
  "Arial", "Calibri", "Cambria", "Consolas", "Courier New", "Georgia", "Segoe UI", "Tahoma", "Times New Roman",
  "Verdana"};

#define FONT_COUNT ( int ) (sizeof(kFonts) / sizeof(kFonts [0]))

static void listbox_row(GalleryApp *g, XtkTxn *txn, XtkNodeRef item, int index) {
	( void ) g;
	ui_text(txn, item, kFonts [index], 14.0f, FLUX_FONT_REGULAR);
}

static char const *listbox_text(GalleryApp *g) {
	return g->model.listbox_sel >= 0 ? gallery_fmt(g, "Font: %s", kFonts [g->model.listbox_sel]) : "Pick a font.";
}

static void listbox_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_LISTBOX_SELECT], listbox_text(g)); }

bool        page_listbox_build(PageBuild *b) {
	page_sync(b, listbox_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "ListBox", "The bordered list surface with accent-filled selection.");
	XtkNodeRef row  = xtk_ref_local(ui_row(
	  &b->txn, page_example(b, parent, "Selection fills the row with the accent at WinUI's 0.6/0.8/0.9 opacities."),
	  16.0f, true
	));
	XtkLocalId list = xtk_listbox_create(
	  &b->txn, row,
	  &(XtkListProps) {
		.set   = XTK_LIST_W_MASK | XTK_LIST_H_MASK | XTK_LIST_SEL_MASK | XTK_LIST_NITEM_MASK | XTK_LIST_ITEMH_MASK,
		.w     = 260.0f,
		.h     = 320.0f,
		.sel   = b->app->model.listbox_sel,
		.nitem = FONT_COUNT,
		.itemh = 40.0f,
	  }
	);
	xtk_listbox_onselect(&b->txn, xtk_ref_local(list), page_post, page_bind(b, MSG_LISTBOX_SELECT));
	page_list(b, list, listbox_row);
	page_keepout(b, MSG_LISTBOX_SELECT, page_note(b, row, listbox_text(b->app)));
	return true;
}
