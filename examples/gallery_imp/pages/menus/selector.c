#include "app.h"

static XtkSelectorItem const kSelectors [] = {
  {.text = "All",    .icon = "List"     },
  {.text = "Music",  .icon = "MusicNote"},
  {.text = "Video",  .icon = "Video"    },
  {.text = "Photos", .icon = "Pictures" },
};

static char const *selector_text(GalleryApp *g) {
	int sel = g->model.selector_sel;
	return sel >= 0 && sel < 4 ? gallery_fmt(g, "Filter: %s", kSelectors [sel].text) : "No filter selected.";
}

static void selector_sync(GalleryApp *g) { gallery_settext(g, g->live_output [MSG_SELECTOR_BAR], selector_text(g)); }

bool        page_selector_bar_build(PageBuild *b) {
	page_sync(b, selector_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "SelectorBar", "A horizontal strip of pill toggles — one selected item at a time.");
	PageDemo   demo = page_demo(b, page_example(b, parent, "Icon+text selector items."));
	XtkLocalId bar  = xtk_selector_create(
	  &b->txn, demo.items,
	  &(XtkSelectionProps) {
		.set   = XTK_SELECTION_SEL_MASK | XTK_SELECTION_NITEM_MASK,
		.sel   = b->app->model.selector_sel,
		.nitem = 4,
	  }
	);
	xtk_txn_setselitems(&b->txn, xtk_ref_local(bar), kSelectors, 4);
	xtk_selector_onselect(&b->txn, xtk_ref_local(bar), page_post, page_bind(b, MSG_SELECTOR_BAR));
	page_keepout(b, MSG_SELECTOR_BAR, page_output(b, demo.row, selector_text(b->app)));
	return true;
}
