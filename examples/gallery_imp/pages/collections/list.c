#include "app.h"

#include <math.h>
#include <stdio.h>

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

static char const *const kWords [] = {
  "layout", "measures", "every", "realized", "row", "while", "the", "rest", "of", "the", "list", "uses", "an",
  "average", "so", "scrolling", "stays", "smooth", "even", "when", "heights", "vary"};

static void note_row(GalleryApp *g, XtkTxn *txn, XtkNodeRef item, int index) {
	char buf [512];
	int  words = 3 + (index * 7) % 41, len = 0;
	for (int i = 0; i < words && len < ( int ) sizeof(buf) - 16; i++)
		len += snprintf(buf + len, sizeof(buf) - ( size_t ) len, i ? " %s" : "%s", kWords [(index + i * 5) % 22]);
	XtkNodeRef col = xtk_ref_local(xtk_container_create(
	  txn, item,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PB_MASK,
		.dir   = XTK_STACK_COLUMN,
		.items = XENT_FLEX_ALIGN_STRETCH,
		.gap   = 2.0f,
		.pt    = 8.0f,
		.pb    = 8.0f,
	  }
	));
	ui_text(txn, col, gallery_fmt(g, "Note %d", index), 14.0f, XTK_FONT_SEMI_BOLD);
	ui_text(txn, col, gallery_fmt(g, "%s.", buf), 13.0f, FLUX_FONT_REGULAR);
}

static int32_t        kContactGroups [26];

static int32_t const *contact_groups(void) {
	for (int g = 0; g < 26; g++) kContactGroups [g] = g % 9 == 8 ? 0 : 2 + (g * 7) % 11;
	return kContactGroups;
}

static int contact_group(int index) {
	int g = 0;
	while (g < 25 && index >= kContactGroups [g]) index -= kContactGroups [g++];
	return g;
}

static void contact_header(GalleryApp *g, XtkTxn *txn, XtkNodeRef item, int group) {
	xtk_text_create(txn, item, &(XtkTextProps) {.set = XTK_TEXT_TEXT_MASK, .text = gallery_fmt(g, "%c", 'A' + group)});
}

static void contact_row(GalleryApp *g, XtkTxn *txn, XtkNodeRef item, int index) {
	char const *first = kFirst [index % (sizeof(kFirst) / sizeof(kFirst [0]))];
	ui_text(
	  txn, item, gallery_fmt(g, "%c. %s (#%d)", 'A' + contact_group(index), first, index), 14.0f, FLUX_FONT_REGULAR
	);
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
	props.set |= XTK_LIST_H_MASK | XTK_LIST_SEL_MASK | XTK_LIST_NITEM_MASK | XTK_LIST_ITEMH_MASK | XTK_LIST_MODE_MASK;
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
	  (XtkListProps) {
		.h = 420.0f, .sel = b->app->model.list_sel, .nitem = 100000, .itemh = 40.0f, .mode = XTK_LIST_SELECT_SINGLE},
	  MSG_LIST_SELECT
	);
	list_example(
	  b, parent,
	  "SelectionMode Extended: Ctrl+click toggles, Shift+click ranges, Ctrl+A selects all. Drag from the empty area "
	  "below the rows to box-select; Ctrl inverts, Shift extends.",
	  (XtkListProps) {
		.set     = XTK_LIST_RECTSEL_MASK,
		.h       = 600.0f,
		.sel     = -1,
		.nitem   = 12,
		.itemh   = 40.0f,
		.mode    = XTK_LIST_SELECT_EXTENDED,
		.rectsel = true},
	  MSG_MULTI_SELECT
	);
	XtkNodeRef ex = page_example(
	  b, parent, "Variable-height rows: 10,000 notes measured as they are realized; resize the window to rewrap."
	);
	XtkLocalId notes = xtk_list_create(
	  &b->txn, ex,
	  &(XtkListProps) {
		.set = XTK_LIST_H_MASK | XTK_LIST_SEL_MASK | XTK_LIST_NITEM_MASK, .h = 360.0f, .sel = -1, .nitem = 10000}
	);
	page_list(b, notes, note_row);
	int32_t const *groups = contact_groups();
	int            total  = 0;
	for (int i = 0; i < 26; i++) total += groups [i];
	XtkLocalId contacts = xtk_list_create(
	  &b->txn,
	  page_example(
		b, parent,
		"Grouped with sticky headers: the current group's header stays pinned until the next one pushes it out."
	  ),
	  &(XtkListProps) {
		.set = XTK_LIST_H_MASK | XTK_LIST_SEL_MASK | XTK_LIST_NITEM_MASK, .h = 360.0f, .sel = -1, .nitem = total}
	);
	page_groups(b, contacts, groups, 26, contact_header, contact_row);
	return true;
}
