#include "app.h"

static char const *const kCrumbs [] = {"Home", "Documents", "Projects", "FluXent"};

static char const       *crumb_text(GalleryApp *g) {
	int click = g->model.breadcrumb_click;
	return click >= 0 && click < 4 ? gallery_fmt(g, "Clicked: %s", kCrumbs [click]) : "Click a crumb to navigate.";
}

static void breadcrumb_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_BREADCRUMB], crumb_text(g)); }

bool        page_breadcrumb_build(PageBuild *b) {
	page_sync(b, breadcrumb_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "BreadcrumbBar", "Shows the path to the current location. Every crumb posts ItemClicked.");
	PageDemo   demo = page_demo(b, page_example(b, parent, "A four-segment breadcrumb."));
	XtkLocalId crumb
	  = xtk_breadcrumb_create(&b->txn, demo.items, &(XtkSelectionProps) {.set = XTK_SELECTION_NITEM_MASK, .nitem = 4});
	xtk_txn_setitems(&b->txn, xtk_ref_local(crumb), kCrumbs, 4);
	xtk_breadcrumb_onselect(&b->txn, xtk_ref_local(crumb), page_post, page_bind(b, MSG_BREADCRUMB));
	page_keepout(b, MSG_BREADCRUMB, page_output(b, demo.row, crumb_text(b->app)));
	return true;
}
