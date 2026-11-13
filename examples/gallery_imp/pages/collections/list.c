#include "app.h"

#include <math.h>

static char const *const kFirst []
  = {"Ada", "Grace", "Alan", "Edsger", "Donald", "Barbara", "John", "Ken", "Dennis", "Bjarne", "Anders", "Lin"};

static char const *const kLast [] = {
  "Lovelace", "Hopper", "Turing", "Dijkstra", "Knuth", "Liskov", "Backus", "Thompson", "Ritchie", "Stroustrup",
  "Hejlsberg", "Xu"};

static void list_row(GalleryApp *g, XtkTxn *txn, XtkNodeRef item, int index) {
	char const *first = kFirst [index % (sizeof(kFirst) / sizeof(kFirst [0]))];
	char const *last  = kLast [(index / 7) % (sizeof(kLast) / sizeof(kLast [0]))];
	XtkNodeRef  row   = xtk_ref_local(ui_row(txn, item, 12.0f, false));
	ui_size(txn, ui_text(txn, row, gallery_fmt(g, "%d", index), 12.0f, FLUX_FONT_REGULAR), 64.0f, NAN);
	ui_text(txn, row, gallery_fmt(g, "%s %s", first, last), 14.0f, FLUX_FONT_REGULAR);
}

static char const *single_text(GalleryApp *g) {
	return g->model.list_sel >= 0 ? gallery_fmt(g, "Selected row: %d", g->model.list_sel) : "Nothing selected yet.";
}

static char const *multi_text(GalleryApp *g) {
	return g->model.multi_lead >= 0 ? gallery_fmt(g, "Lead selection: row %d", g->model.multi_lead)
	                                : "Selection is control-retained; the lead index posts here.";
}

static void listview_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_LIST_SELECT], single_text(g));
	gallery_settext(g, g->live_output [MSG_MULTI_SELECT], multi_text(g));
}

static void list_example(PageBuild *b, XtkNodeRef parent, char const *caption, XtkListProps props, int tag) {
	XtkNodeRef col
	  = xtk_ref_local(ui_column(&b->txn, page_example(b, parent, caption), 12.0f, XENT_FLEX_ALIGN_STRETCH));
	props.set   |= XTK_LIST_H_MASK | XTK_LIST_SEL_MASK | XTK_LIST_NITEM_MASK | XTK_LIST_ITEMH_MASK | XTK_LIST_MODE_MASK;
	props.itemh  = 40.0f;
	XtkLocalId list = xtk_list_create(&b->txn, col, &props);
	xtk_list_onselect(&b->txn, xtk_ref_local(list), page_post, page_bind(b, tag));
	page_list(b, list, list_row);
	page_keepout(b, tag, page_note(b, col, tag == MSG_LIST_SELECT ? single_text(b->app) : multi_text(b->app)));
}

bool page_listview_build(PageBuild *b) {
	page_sync(b, listview_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "ListView",
	  "A virtualized list: 100,000 rows, but only the viewport's worth of controls exist. Scrolling recycles rows in "
	  "place."
	);
	list_example(
	  b, parent, "Click a row to select it.",
	  (XtkListProps) {.h = 420.0f, .sel = b->app->model.list_sel, .nitem = 100000, .mode = XTK_LIST_SELECT_SINGLE},
	  MSG_LIST_SELECT
	);
	list_example(
	  b, parent, "SelectionMode Extended: Ctrl+click toggles, Shift+click ranges, Ctrl+A selects all.",
	  (XtkListProps) {.h = 240.0f, .sel = -1, .nitem = 200, .mode = XTK_LIST_SELECT_EXTENDED}, MSG_MULTI_SELECT
	);
	return true;
}
