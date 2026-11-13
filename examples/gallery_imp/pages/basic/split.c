#include "app.h"

static char const *const kActions [] = {"(nothing yet)", "Saved", "Saved as...", "Saved all"};

static char const       *split_text(GalleryApp *g) {
	int action = g->model.split_action >= 0 && g->model.split_action <= 3 ? g->model.split_action : 0;
	return gallery_fmt(g, "Last action: %s", kActions [action]);
}

static void split_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_SPLIT_PRIMARY], split_text(g)); }

static void menu_item(PageBuild *b, XtkNodeRef menu, char const *text, int tag, int i) {
	XtkLocalId item = xtk_menuitem_create(&b->txn, menu, &(XtkItemProps) {.set = XTK_ITEM_TEXT_MASK, .text = text});
	xtk_menuitem_oninvoke(&b->txn, xtk_ref_local(item), page_post, page_bind_i(b, tag, i));
}

bool page_split_build(PageBuild *b) {
	page_sync(b, split_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "SplitButton", "The left zone fires the default action; the chevron opens a menu of alternatives."
	);
	PageDemo   demo  = page_demo(b, page_example(b, parent, "A SplitButton for saving with alternatives."));
	XtkLocalId split = xtk_split_create(
	  &b->txn, demo.items,
	  &(XtkButtonProps) {.set = XTK_BUTTON_TEXT_MASK | XTK_BUTTON_ICON_MASK, .text = "Save", .icon = "Save"}
	);
	xtk_split_oninvoke(&b->txn, xtk_ref_local(split), page_post, page_bind(b, MSG_SPLIT_PRIMARY));
	XtkNodeRef menu = xtk_ref_local(xtk_menufly_create(&b->txn, xtk_ref_local(split), NULL));
	menu_item(b, menu, "Save as...", MSG_SPLIT_MENU, 0);
	menu_item(b, menu, "Save all", MSG_SPLIT_MENU, 1);
	page_keepout(b, MSG_SPLIT_PRIMARY, page_output(b, demo.row, split_text(b->app)));
	return true;
}
