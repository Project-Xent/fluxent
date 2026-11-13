#include "app.h"

static void expander_sync(GalleryApp *g) {
	xtk_expander_setexpanded(&g->scene, g->live_control [MSG_EXPAND], g->model.expanded, (XtkChangeStamp) {0});
	xtk_checkbox_setchecked(&g->scene, g->live_control [MSG_CHECK_A], g->model.check_a, (XtkChangeStamp) {0});
}

bool page_expander_build(PageBuild *b) {
	page_sync(b, expander_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "Expander", "An Expander shows a header and reveals a content region when toggled.");
	XtkNodeRef ex  = page_example(b, parent, "An Expander with interactive content.");
	XtkLocalId exp = xtk_expander_create(
	  &b->txn, ex,
	  &(XtkExpanderProps) {
		.set      = XTK_EXPANDER_TEXT_MASK | XTK_EXPANDER_EXPANDED_MASK,
		.text     = "This Expander is bound to the model",
		.expanded = b->app->model.expanded,
	  }
	);
	XtkNodeRef ref = xtk_ref_local(exp);
	xtk_expander_ontoggle(&b->txn, ref, page_post, page_bind(b, MSG_EXPAND));
	ui_text(&b->txn, ref, "Anything can live in here — it is a normal container.", 14.0f, FLUX_FONT_REGULAR);
	XtkLocalId box
	  = page_checkbox(b, ref, "Even this checkbox from the CheckBox page", b->app->model.check_a, MSG_CHECK_A);
	page_keep(b, MSG_EXPAND, exp);
	page_keep(b, MSG_CHECK_A, box);
	return true;
}
