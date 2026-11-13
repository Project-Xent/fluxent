#include "app.h"
#include <string.h>

static void page_scroll_bar_sync(GalleryApp *g);

static void scroll_bar_example(
  PageBuild *b, XtkNodeRef parent, char const *caption, int tag, double value, XtkScrollBarProps extra
) {
	XtkNodeRef ex    = page_example(b, parent, caption);
	PageDemo   demo  = page_demo(b, ex);
	extra.set       |= XTK_SCROLLBAR_VALUE_MASK;
	extra.value      = value;
	XtkLocalId bar   = xtk_scrollbar_create(&b->txn, demo.items, &extra);
	if (tag == 0) return;
	xtk_scrollbar_onchange(&b->txn, xtk_ref_local(bar), page_post, page_bind(b, tag));
	XtkLocalId output = page_output(b, demo.row, gallery_fmt(b->app, "%.0f", value));
	page_keepout(b, tag, output);
}

bool page_scroll_bar_build(PageBuild *b) {
	page_sync(b, page_scroll_bar_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "ScrollBar",
	  "A ScrollBar reports where the user asked to scroll to; it moves nothing on its own. "
	  "ViewportSize is in the same units as the range and decides how much of the track the thumb covers."
	);
	scroll_bar_example(
	  b, parent, "A vertical ScrollBar over a range of 1000 with a viewport of 200.", MSG_SCROLLBAR_V, m->scrollbar_v,
	  (XtkScrollBarProps) {
		.set      = XTK_SCROLLBAR_H_MASK | XTK_SCROLLBAR_MAX_MASK | XTK_SCROLLBAR_VIEWPORT_MASK,
		.h        = 200.0f,
		.max      = 1000.0,
		.viewport = 200.0,
	  }
	);
	scroll_bar_example(
	  b, parent, "A horizontal ScrollBar. Page steps come from LargeChange, the line buttons from SmallChange.",
	  MSG_SCROLLBAR_H, m->scrollbar_h,
	  (XtkScrollBarProps) {
		.set         = XTK_SCROLLBAR_W_MASK
	                 | XTK_SCROLLBAR_DIR_MASK
	                 | XTK_SCROLLBAR_MAX_MASK
	                 | XTK_SCROLLBAR_VIEWPORT_MASK
	                 | XTK_SCROLLBAR_SMALLCHANGE_MASK
	                 | XTK_SCROLLBAR_LARGECHANGE_MASK,
		.w           = 300.0f,
		.dir         = XTK_STACK_ROW,
		.max         = 500.0,
		.viewport    = 100.0,
		.smallchange = 5.0f,
		.largechange = 100.0f,
	  }
	);
	scroll_bar_example(
	  b, parent, "A disabled ScrollBar.", 0, 300.0,
	  (XtkScrollBarProps) {
		.set = XTK_SCROLLBAR_H_MASK | XTK_SCROLLBAR_MAX_MASK | XTK_SCROLLBAR_VIEWPORT_MASK | XTK_SCROLLBAR_ENABLED_MASK,
		.h   = 200.0f,
		.max = 1000.0,
		.viewport = 200.0,
		.enabled  = false,
	  }
	);
	return true;
}

static void scroll_bar_text(GalleryApp *g, int tag, double value) {
	char const *text = gallery_fmt(g, "%.0f", value);
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [tag], text, ( uint32_t ) strlen(text), (XtkChangeStamp) {0}
	);
}

static void page_scroll_bar_sync(GalleryApp *g) {
	scroll_bar_text(g, MSG_SCROLLBAR_V, g->model.scrollbar_v);
	scroll_bar_text(g, MSG_SCROLLBAR_H, g->model.scrollbar_h);
}
