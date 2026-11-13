#include "app.h"

static void flip_page(GalleryApp *g, XtkTxn *txn, XtkNodeRef parent, int i) {
	static char const *const kTitles [] = {"Sunrise", "Forest", "Ocean", "Dunes", "Aurora"};
	XtkLocalId               card       = ui_card(txn, parent, 0.0f, XENT_FLEX_ALIGN_CENTER, 16.0f);
	ui_center(txn, card);
	ui_text(txn, xtk_ref_local(card), kTitles [i], 32.0f, FLUX_FONT_SEMI_BOLD);
	ui_text(txn, xtk_ref_local(card), gallery_fmt(g, "Page %d of 5", i + 1), 13.0f, FLUX_FONT_REGULAR);
}

static void flip_sync(GalleryApp *g) {
	xtk_flip_setsel(&g->scene, g->live_control [MSG_FLIP_SELECT], g->model.flip_page, (XtkChangeStamp) {0});
	xtk_pips_setsel(&g->scene, g->live_output [MSG_FLIP_SELECT], g->model.flip_page, (XtkChangeStamp) {0});
}

bool page_flipview_build(PageBuild *b) {
	page_sync(b, flip_sync);
	int        sel    = b->app->model.flip_page;
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "FlipView",
	  "One page at a time: hover for the nav arrows, use the wheel or arrow keys, or drive it from the PipsPager below "
	  "— both post the same message."
	);
	XtkNodeRef col  = xtk_ref_local(ui_column(
	  &b->txn, page_example(b, parent, "FlipView + PipsPager sharing one selected index."), 12.0f,
	  XENT_FLEX_ALIGN_CENTER
	));
	XtkLocalId flip = xtk_flip_create(
	  &b->txn, col,
	  &(XtkFlipProps) {
		.set = XTK_FLIP_W_MASK | XTK_FLIP_H_MASK | XTK_FLIP_SEL_MASK, .w = 560.0f, .h = 300.0f, .sel = sel}
	);
	for (int i = 0; i < 5; i++) flip_page(b->app, &b->txn, xtk_ref_local(flip), i);
	xtk_flip_onselect(&b->txn, xtk_ref_local(flip), page_post, page_bind(b, MSG_FLIP_SELECT));
	XtkLocalId pips = xtk_pips_create(
	  &b->txn, col,
	  &(XtkPipsProps) {
		.set   = XTK_PIPS_SEL_MASK | XTK_PIPS_NITEM_MASK | XTK_PIPS_NAV_MASK,
		.sel   = sel,
		.nitem = 5,
		.nav   = XTK_PIPS_NAV_ON_HOVER,
	  }
	);
	xtk_pips_onselect(&b->txn, xtk_ref_local(pips), page_post, page_bind(b, MSG_FLIP_SELECT));
	page_keep(b, MSG_FLIP_SELECT, flip);
	page_keepout(b, MSG_FLIP_SELECT, pips);
	return true;
}
