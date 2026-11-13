#include "app.h"

static char const *const kMenuOut [] = {"(primary click)", "Align left", "Align center", "Align right"};

static char const       *toggle_split_text(GalleryApp *g) {
	int menu = g->model.toggle_split_menu >= 0 && g->model.toggle_split_menu <= 3 ? g->model.toggle_split_menu : 0;
	return gallery_fmt(g, "%s — %s", g->model.toggle_split_checked ? "On" : "Off", kMenuOut [menu]);
}

static void toggle_split_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_TOGGLE_SPLIT], toggle_split_text(g));
}

bool page_toggle_split_build(PageBuild *b) {
	page_sync(b, toggle_split_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "ToggleSplitButton",
	  "The left zone toggles checked state; the chevron opens a menu of secondary actions."
	);
	PageDemo   demo  = page_demo(b, page_example(b, parent, "Toggle + menu split button."));
	XtkLocalId split = xtk_togglesplit_create(
	  &b->txn, demo.items,
	  &(XtkToggleProps) {
		.set     = XTK_TOGGLE_TEXT_MASK | XTK_TOGGLE_CHECKED_MASK,
		.text    = "Bold",
		.checked = b->app->model.toggle_split_checked,
	  }
	);
	xtk_txn_set(
	  &b->txn, xtk_ref_local(split), XTK_PROP_ICON,
	  (XtkValue) {
		.kind = XTK_VALUE_STRING, .u.string = {.data = ( char * ) "Bold", .len = 4}
    },
	  (XtkChangeStamp) {0}
	);
	xtk_togglesplit_onchange(&b->txn, xtk_ref_local(split), page_post, page_bind(b, MSG_TOGGLE_SPLIT));
	page_on(b, split, XTK_EVENT_INVOKE, MSG_TOGGLE_SPLIT_CLICK, 0);
	XtkNodeRef               menu      = xtk_ref_local(xtk_menufly_create(&b->txn, xtk_ref_local(split), NULL));
	static char const *const kItems [] = {"Align left", "Align center", "Align right"};
	for (int i = 0; i < 3; i++) {
		XtkLocalId item
		  = xtk_menuitem_create(&b->txn, menu, &(XtkItemProps) {.set = XTK_ITEM_TEXT_MASK, .text = kItems [i]});
		xtk_menuitem_oninvoke(&b->txn, xtk_ref_local(item), page_post, page_bind_i(b, MSG_TOGGLE_SPLIT_MENU, i));
	}
	page_keepout(b, MSG_TOGGLE_SPLIT, page_output(b, demo.row, toggle_split_text(b->app)));
	return true;
}
