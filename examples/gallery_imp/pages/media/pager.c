#include "app.h"

static void pager_sync(GalleryApp *g) {
	xtk_pager_setsel(&g->scene, g->live_control [MSG_PAGER_SELECT], g->model.pager_page, (XtkChangeStamp) {0});
	xtk_pager_setsel(&g->scene, g->live_extra [0], g->model.pager_page, (XtkChangeStamp) {0});
	gallery_settext(g, g->live_output [MSG_PAGER_SELECT], gallery_fmt(g, "Page %d of 32", g->model.pager_page + 1));
}

static XtkLocalId pager(PageBuild *b, XtkNodeRef parent, XtkPagerProps props) {
	props.set    |= XTK_PAGER_SEL_MASK | XTK_PAGER_NITEM_MASK;
	props.sel     = b->app->model.pager_page;
	props.nitem   = 32;
	XtkLocalId p  = xtk_pager_create(&b->txn, parent, &props);
	xtk_pager_onselect(&b->txn, xtk_ref_local(p), page_post, page_bind(b, MSG_PAGER_SELECT));
	return p;
}

bool page_pager_build(PageBuild *b) {
	page_sync(b, pager_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "PagerControl",
	  "A numeric/arrow pager (distinct from the dot PipsPager): First/Previous/Next/Last buttons around a number panel "
	  "with ellipses, or a compact page-of-total label."
	);
	XtkNodeRef card = xtk_ref_local(ui_card(
	  &b->txn, page_example(b, parent, "Number panel (32 pages) with First/Last buttons."), 12.0f,
	  XENT_FLEX_ALIGN_START, 16.0f
	));
	page_keep(
	  b, MSG_PAGER_SELECT,
	  pager(
		b, card,
		(XtkPagerProps) {
		  .set   = XTK_PAGER_FIRST_MASK | XTK_PAGER_LAST_MASK,
		  .first = XTK_PAGER_BUTTON_AUTO,
		  .last  = XTK_PAGER_BUTTON_AUTO,
		}
	  )
	);
	page_keepout(
	  b, MSG_PAGER_SELECT, page_note(b, card, gallery_fmt(b->app, "Page %d of 32", b->app->model.pager_page + 1))
	);
	page_keepx(
	  b, 0,
	  pager(
		b, page_example(b, parent, "Number-text display."),
		(XtkPagerProps) {
		  .set     = XTK_PAGER_DISPLAY_MASK | XTK_PAGER_PREFIX_MASK | XTK_PAGER_SUFFIX_MASK,
		  .display = XTK_PAGER_DISPLAY_NUMBER_TEXT,
		  .prefix  = "Page",
		  .suffix  = "of",
		}
	  )
	);
	return true;
}
